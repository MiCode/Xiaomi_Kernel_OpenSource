// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/genalloc.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fb.h>
#include <mt-plat/mtk_gpu_utility.h>

#include "ged_base.h"
#include "ged_hal.h"
#ifdef GED_DEBUG_FS
#include "ged_debugFS.h"
#endif
#include "ged_sysfs.h"

#include "ged_dvfs.h"

#include "ged_notify_sw_vsync.h"
#include "ged_kpi.h"
#include "ged_global.h"

#ifdef GED_DEBUG_FS
static struct dentry *gpsHALDir;
static struct dentry *gpsTotalGPUFreqLevelCountEntry;
static struct dentry *gpsCustomBoostGPUFreqEntry;
static struct dentry *gpsCustomUpboundGPUFreqEntry;
static struct dentry *gpsVsyncOffsetEnableEntry;
static struct dentry *gpsVsyncOffsetLevelEntry;
static struct dentry *gpsGPUInfoEntry;
static struct dentry *gpsDvfsTuningModeEntry;
static struct dentry *gpsDvfsCurFreqEntry;
static struct dentry *gpsDvfsPreFreqEntry;
static struct dentry *gpsDvfsGpuUtilizationEntry;
static struct dentry *gpsFpsUpperBoundEntry;
static struct dentry *gpsBoostLevelEntry;
#ifdef MTK_GED_KPI
static struct dentry *gpsGedInfoKPIEntry;
#endif
#if (defined(GED_ENABLE_FB_DVFS) && defined(GED_ENABLE_DYNAMIC_DVFS_MARGIN))
static struct dentry *gpsDvfsMarginValueEntry;
#endif
#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
static struct dentry *gpsLoadingBaseDvfsStepEntry;
#endif
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
static struct dentry *gpsTimerBaseDvfsMarginEntry;
#endif
#ifdef GED_ENABLE_DVFS_LOADING_MODE
static struct dentry *gpsDvfsLoadingModeEntry;
#endif
static struct dentry *gpsIntegrationReportReadEntry;
#endif /* GED_DEBUG_FS */

static struct kobject *hal_kobj;

int tokenizer(char *pcSrc, int i32len, int *pi32IndexArray, int i32NumToken)
{
	int i = 0;
	int j = 0;
	int head = -1;

	for ( ; i < i32len; i++) {
		if (pcSrc[i] != ' ') {
			if (head == -1)
				head = i;
		} else {
			if (head != -1) {
				pi32IndexArray[j] = head;
				j++;
				if (j == i32NumToken)
					return j;
				head = -1;
			}
			pcSrc[i] = 0;
		}
	}

	if (head != -1) {
		pi32IndexArray[j] = head;
		j++;
		return j;
	}

	return -1;
}

//-----------------------------------------------------------------------------
#ifdef GED_DEBUG_FS
static void *ged_total_gpu_freq_level_count_seq_start(
	struct seq_file *psSeqFile, loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_total_gpu_freq_level_count_seq_stop(struct seq_file *psSeqFile,
	void *pvData)
{

}

static void *ged_total_gpu_freq_level_count_seq_next(struct seq_file *psSeqFile,
	void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_total_gpu_freq_level_count_seq_show(struct seq_file *psSeqFile,
	void *pvData)
{
	if (pvData != NULL) {
		unsigned int ui32FreqLevelCount;

		if (mtk_custom_get_gpu_freq_level_count(&ui32FreqLevelCount) ==
			false) {
			ui32FreqLevelCount = 0;
		}
		seq_printf(psSeqFile, "%u\n", ui32FreqLevelCount);
	}

	return 0;
}

static const struct seq_operations gsTotalGPUFreqLevelCountReadOps = {
	.start = ged_total_gpu_freq_level_count_seq_start,
	.stop = ged_total_gpu_freq_level_count_seq_stop,
	.next = ged_total_gpu_freq_level_count_seq_next,
	.show = ged_total_gpu_freq_level_count_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
static ssize_t total_gpu_freq_level_count_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32FreqLevelCount;

	if (false == mtk_custom_get_gpu_freq_level_count(&ui32FreqLevelCount))
		ui32FreqLevelCount = 0;

	return scnprintf(buf, PAGE_SIZE, "%u\n", ui32FreqLevelCount);
}

static KOBJ_ATTR_RO(total_gpu_freq_level_count);
//-----------------------------------------------------------------------------
#ifdef GED_DEBUG_FS
static ssize_t ged_custom_boost_gpu_freq_write_entry(
	const char __user *pszBuffer, size_t uiCount,
	loff_t uiPosition, void *pvData)
{
#define GED_HAL_DEBUGFS_SIZE 64
	char acBuffer[GED_HAL_DEBUGFS_SIZE];

	int i32Value;

	if ((uiCount > 0) && (uiCount < GED_HAL_DEBUGFS_SIZE)) {
		if (ged_copy_from_user(acBuffer, pszBuffer, uiCount) == 0) {
			acBuffer[uiCount] = '\0';
			if (kstrtoint(acBuffer, 10, &i32Value) == 0) {
				if (i32Value < 0)
					i32Value = 0;
				mtk_custom_boost_gpu_freq(i32Value);
			}
		}
	}

	return uiCount;
}

static void *ged_custom_boost_gpu_freq_seq_start(struct seq_file *psSeqFile,
	loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_custom_boost_gpu_freq_seq_stop(struct seq_file *psSeqFile,
	void *pvData)
{

}

static void *ged_custom_boost_gpu_freq_seq_next(struct seq_file *psSeqFile,
	void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_custom_boost_gpu_freq_seq_show(struct seq_file *psSeqFile,
	void *pvData)
{
	if (pvData != NULL) {
		unsigned int ui32BoostGpuFreqLevel;

		if (mtk_get_custom_boost_gpu_freq(&ui32BoostGpuFreqLevel) ==
			false) {
			ui32BoostGpuFreqLevel = 0;
		}
		seq_printf(psSeqFile, "%u\n", ui32BoostGpuFreqLevel);
	}

	return 0;
}

static const struct seq_operations gsCustomBoostGpuFreqReadOps = {
	.start = ged_custom_boost_gpu_freq_seq_start,
	.stop = ged_custom_boost_gpu_freq_seq_stop,
	.next = ged_custom_boost_gpu_freq_seq_next,
	.show = ged_custom_boost_gpu_freq_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
static ssize_t custom_boost_gpu_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32BoostGpuFreqLevel;

	if (false == mtk_get_custom_boost_gpu_freq(&ui32BoostGpuFreqLevel))
		ui32BoostGpuFreqLevel = 0;

	return scnprintf(buf, PAGE_SIZE, "%u\n", ui32BoostGpuFreqLevel);
}

static ssize_t custom_boost_gpu_freq_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value < 0)
					i32Value = 0;
				mtk_custom_boost_gpu_freq(i32Value);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(custom_boost_gpu_freq);
//-----------------------------------------------------------------------------
#ifdef GED_DEBUG_FS
static ssize_t ged_custom_upbound_gpu_freq_write_entry(
	const char __user *pszBuffer, size_t uiCount,
	loff_t uiPosition, void *pvData)
{
#define GED_HAL_DEBUGFS_SIZE 64
	char acBuffer[GED_HAL_DEBUGFS_SIZE];

	int i32Value;

	if ((uiCount > 0) && (uiCount < GED_HAL_DEBUGFS_SIZE)) {
		if (ged_copy_from_user(acBuffer, pszBuffer, uiCount) == 0) {
			acBuffer[uiCount] = '\0';
			if (kstrtoint(acBuffer, 10, &i32Value) == 0) {
				if (i32Value < 0)
					i32Value = 0;
				mtk_custom_upbound_gpu_freq(i32Value);
			}
		}
	}

	return uiCount;
}

static void *ged_custom_upbound_gpu_freq_seq_start(struct seq_file *psSeqFile,
	loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;
	return NULL;
}

static void ged_custom_upbound_gpu_freq_seq_stop(struct seq_file *psSeqFile,
	void *pvData)
{

}

static void *ged_custom_upbound_gpu_freq_seq_next(struct seq_file *psSeqFile,
	void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_custom_upbound_gpu_freq_seq_show(struct seq_file *psSeqFile,
	void *pvData)
{
	if (pvData != NULL) {
		unsigned int ui32UpboundGpuFreqLevel;

		if (mtk_get_custom_upbound_gpu_freq(&ui32UpboundGpuFreqLevel) ==
			false) {
			ui32UpboundGpuFreqLevel = 0;
			seq_printf(psSeqFile,
				"call mtk_get_custom_upbound_gpu_freq false\n");
		}
		seq_printf(psSeqFile, "%u\n", ui32UpboundGpuFreqLevel);
	}

	return 0;
}

const struct seq_operations gsCustomUpboundGpuFreqReadOps = {
	.start = ged_custom_upbound_gpu_freq_seq_start,
	.stop = ged_custom_upbound_gpu_freq_seq_stop,
	.next = ged_custom_upbound_gpu_freq_seq_next,
	.show = ged_custom_upbound_gpu_freq_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
static ssize_t custom_upbound_gpu_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32UpboundGpuFreqLevel;
	char temp[GED_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	if (false == mtk_get_custom_upbound_gpu_freq(
			&ui32UpboundGpuFreqLevel)) {
		ui32UpboundGpuFreqLevel = 0;
		length = scnprintf(temp + pos, GED_SYSFS_MAX_BUFF_SIZE - pos,
				"call mtk_get_custom_upbound_gpu_freq false\n");
		pos += length;
	}
	length = scnprintf(temp + pos, GED_SYSFS_MAX_BUFF_SIZE - pos,
			"%u\n", ui32UpboundGpuFreqLevel);
	pos += length;

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t custom_upbound_gpu_freq_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value < 0)
					i32Value = 0;
				mtk_custom_upbound_gpu_freq(i32Value);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(custom_upbound_gpu_freq);
//-----------------------------------------------------------------------------
#ifdef GED_DEBUG_FS
static bool bForce = GED_FALSE;
static ssize_t ged_vsync_offset_enable_write_entry(const char __user *pszBuffer,
	size_t uiCount, loff_t uiPosition, void *pvData)
{
#define GED_HAL_DEBUGFS_SIZE 64
#define NUM_TOKEN 2

	/*
	 *  This proc node accept only: [CMD] [NUM]
	 *  for ex: "touch 1"
	 */

	char acBuffer[GED_HAL_DEBUGFS_SIZE];
	int aint32Indx[NUM_TOKEN];
	char *pcCMD;
	char *pcValue;
	int value;
	int i;

	if (uiCount >= GED_HAL_DEBUGFS_SIZE)
		goto normal_exit;

	if (ged_copy_from_user(acBuffer, pszBuffer, uiCount))
		goto normal_exit;

	for (i = 0; i < NUM_TOKEN; ++i)
		aint32Indx[i] = 0;

	acBuffer[uiCount] = '\0';
	i = tokenizer(acBuffer, uiCount, aint32Indx, NUM_TOKEN);
	if (i != NUM_TOKEN)
		goto normal_exit;

	pcCMD = acBuffer + aint32Indx[0];
	pcValue = acBuffer + aint32Indx[1];

	value = (pcValue[0] - '0');

	if (strcmp(pcCMD, "touch_down") == 0) {
		ged_dvfs_vsync_offset_event_switch(
			GED_DVFS_VSYNC_OFFSET_TOUCH_EVENT, !!value);
	} else if (strcmp(pcCMD, "enable_WFD") == 0) {
		ged_dvfs_vsync_offset_event_switch(
			GED_DVFS_VSYNC_OFFSET_WFD_EVENT, !!value);
	} else if (strcmp(pcCMD, "enable_debug") == 0) {
		if (value == 1) {
			ged_dvfs_vsync_offset_event_switch(
				GED_DVFS_VSYNC_OFFSET_FORCE_OFF, true);
			bForce = GED_FALSE;
		} else if (value == 2) {
			ged_dvfs_vsync_offset_event_switch(
				GED_DVFS_VSYNC_OFFSET_FORCE_ON, true);
			bForce = GED_TRUE;
		} else if (value == 0)
			ged_dvfs_vsync_offset_event_switch(
			GED_DVFS_VSYNC_OFFSET_DEBUG_CLEAR_EVENT, true);
	} else if (strcmp(pcCMD, "gas") == 0) {
		ged_dvfs_vsync_offset_event_switch(
			GED_DVFS_VSYNC_OFFSET_GAS_EVENT, !!value);
		ged_kpi_set_game_hint(!!value ? 1 : 0);
	} else if (strcmp(pcCMD, "enable_VR") == 0) {
		ged_dvfs_vsync_offset_event_switch(
			GED_DVFS_VSYNC_OFFSET_VR_EVENT, !!value);
	} else if (strcmp(pcCMD, "mhl4k-vid") == 0) {
		ged_dvfs_vsync_offset_event_switch(
			GED_DVFS_VSYNC_OFFSET_MHL4K_VID_EVENT, !!value);
	} else if (strcmp(pcCMD, "low-power-mode") == 0) {
		ged_dvfs_vsync_offset_event_switch(
			GED_DVFS_VSYNC_OFFSET_LOW_POWER_MODE_EVENT, !!value);
	} else if (strcmp(pcCMD, "low_latency_mode") == 0) {
		ged_dvfs_vsync_offset_event_switch
		(GED_DVFS_VSYNC_OFFSET_LOW_LATENCY_MODE_EVENT, !!value);
	} else if (strcmp(pcCMD, "video-merge-md") == 0) {
		if (value == 1)
			ged_dvfs_vsync_offset_event_switch(
				GED_DVFS_VSYNC_OFFSET_FORCE_OFF, true);
		else if (value == 0)
			ged_dvfs_vsync_offset_event_switch(
				GED_DVFS_VSYNC_OFFSET_DEBUG_CLEAR_EVENT, true);
	} else
		GED_LOGE("Unknown command:%s %c", pcCMD, *pcValue);

normal_exit:
	return uiCount;

}

static void *ged_vsync_offset_enable_seq_start(struct seq_file *psSeqFile,
	loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_vsync_offset_enable_seq_stop(struct seq_file *psSeqFile,
	void *pvData)
{

}

static void *ged_vsync_offset_enable_seq_next(struct seq_file *psSeqFile,
	void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_vsync_offset_enable_seq_show(struct seq_file *psSeqFile,
	void *pvData)
{
	if (pvData != NULL) {
		seq_printf(psSeqFile, "g_ui32EventStatus =%x\n",
			g_ui32EventStatus);
		seq_printf(psSeqFile, "g_ui32EventDebugStatus =%x\n",
			g_ui32EventDebugStatus);
		if (g_ui32EventDebugStatus & GED_EVENT_FORCE_ON)
			seq_puts(psSeqFile, "Debug mode: Force on\n");
		else if (g_ui32EventDebugStatus & GED_EVENT_FORCE_OFF)
			seq_puts(psSeqFile, "Debug mode: Force off\n");
		else {
			seq_printf(psSeqFile, "Touch: %d\n",
				g_ui32EventStatus & GED_EVENT_TOUCH ? 1 : 0);
			seq_printf(psSeqFile, "WFD: %d\n",
				g_ui32EventStatus & GED_EVENT_WFD ? 1 : 0);
			seq_printf(psSeqFile, "MHL: %d\n",
				g_ui32EventStatus & GED_EVENT_MHL ? 1 : 0);
			seq_printf(psSeqFile, "GAS: %d\n",
				g_ui32EventStatus & GED_EVENT_GAS ? 1 : 0);
			seq_printf(psSeqFile, "VR: %d\n",
				g_ui32EventStatus & GED_EVENT_VR ? 1 : 0);
			seq_printf(psSeqFile, "Thermal: %d\n",
				g_ui32EventStatus & GED_EVENT_THERMAL ? 1 : 0);
			seq_printf(psSeqFile, "Low power mode: %d\n",
			g_ui32EventStatus & GED_EVENT_LOW_POWER_MODE ? 1 : 0);
			seq_printf(psSeqFile, "MHL4K Video: %d\n",
			g_ui32EventStatus & GED_EVENT_MHL4K_VID ? 1 : 0);
			seq_printf(psSeqFile, "LCD: %d\n",
				g_ui32EventStatus & GED_EVENT_LCD ? 1 : 0);
			seq_printf(psSeqFile, "Low latency mode: %d\n",
			g_ui32EventStatus&GED_EVENT_LOW_LATENCY_MODE ? 1 : 0);
			seq_printf(psSeqFile, "dHWC: %d\n",
				g_ui32EventStatus & GED_EVENT_DHWC ? 1 : 0);
		}
	}

	return 0;
}

static const struct seq_operations gsVsync_offset_enableReadOps = {
	.start = ged_vsync_offset_enable_seq_start,
	.stop = ged_vsync_offset_enable_seq_stop,
	.next = ged_vsync_offset_enable_seq_next,
	.show = ged_vsync_offset_enable_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
#ifdef GED_DEBUG_FS
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
	 */

	char acBuffer[GED_HAL_DEBUGFS_SIZE];
	int aint32Indx[NUM_TOKEN];
	char *pcCMD;
	char *pcValue;
	int i;
	int i32VsyncOffsetLevel;
	int ret;

	if (!((uiCount > 0) && (uiCount < GED_HAL_DEBUGFS_SIZE - 1)))
		return 0;

	if (ged_copy_from_user(acBuffer, pszBuffer, uiCount))
		return 0;

	for (i = 0; i < NUM_TOKEN; ++i)
		aint32Indx[i] = 0;

	acBuffer[uiCount] = '\n';
	acBuffer[uiCount+1] = 0;
	i = tokenizer(acBuffer, uiCount, aint32Indx, NUM_TOKEN);
	GED_LOGD("i=%d", i);
	if (i == NUM_TOKEN) {
		pcCMD = acBuffer+aint32Indx[0];

		pcValue = acBuffer+aint32Indx[1];
		if (strcmp(pcCMD, "set_vsync_offset") == 0) {
			ret = kstrtoint(pcValue, 0, &i32VsyncOffsetLevel);
			ged_dvfs_vsync_offset_level_set(i32VsyncOffsetLevel);
		}
	}

	return 0;
}

static void *ged_vsync_offset_level_seq_start(struct seq_file *psSeqFile,
	loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;


	return NULL;
}

static void ged_vsync_offset_level_seq_stop(struct seq_file *psSeqFile,
	void *pvData)
{

}

static void *ged_vsync_offset_level_seq_next(struct seq_file *psSeqFile,
	void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_vsync_offset_level_seq_show(struct seq_file *psSeqFile,
	void *pvData)
{
	if (pvData != NULL) {
		seq_printf(psSeqFile, "%d\n",
			ged_dvfs_vsync_offset_level_get());
	}

	return 0;
}

static const struct seq_operations gsVsync_offset_levelReadOps = {
	.start = ged_vsync_offset_level_seq_start,
	.stop = ged_vsync_offset_level_seq_stop,
	.next = ged_vsync_offset_level_seq_next,
	.show = ged_vsync_offset_level_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
#ifdef GED_DEBUG_FS
void ged_gpu_info_dump_cap(struct seq_file *psSeqFile, bool bHumanReadable)
{
	char buf[256];
	int cx;

	if (bHumanReadable) {
		seq_puts(psSeqFile, "Name = MT5566\n");
		cx = snprintf(buf, sizeof(buf), "ALU_CAPABILITY = %d\n", 5566);
		if (cx < 0 || cx >= sizeof(buf))
			return;
		seq_puts(psSeqFile, buf);
		cx = snprintf(buf, sizeof(buf), "TEX_CAPABILITY = %d\n", 7788);
		if (cx < 0 || cx >= sizeof(buf))
			return;
		seq_puts(psSeqFile, buf);
	} else {
		cx = snprintf(buf, sizeof(buf), "MT5566, %d, %d\n", 5566, 7788);
		if (cx < 0 || cx >= sizeof(buf))
			return;
		seq_puts(psSeqFile, buf);
	}
}

void ged_gpu_info_profile(struct seq_file *psSeqFile, bool bHumanReadable)
{
	if (bHumanReadable) {
		seq_printf(psSeqFile, "GPU_CAP_USED = %d\n", 10);
		seq_printf(psSeqFile, "UTILIZATION = %d\n", 300);
		seq_printf(psSeqFile, "ALU_URATE = %d\n", 300);
		seq_printf(psSeqFile, "TEX_URATE = %d\n", 300);
		seq_printf(psSeqFile, "BW_URATE = %d\n", 300);
		seq_printf(psSeqFile, "VERTEX_SHADER_URATE = %d\n", 300);
		seq_printf(psSeqFile, "PIXEL_SHADER_URATE = %d\n", 300);
	} else {
		seq_printf(psSeqFile, "%d, %d, %d, %d, %d, %d, %d, %d\n"
		, 7, 10, 300, 300, 300, 300, 300, 300);
	}
}

void ged_get_gpu_info(struct seq_file *psSeqFile, int i32Mode)
{
	static unsigned int ui32mode = GED_GPU_INFO_CAPABILITY;
	static bool bHumanReadable;

	if (psSeqFile) { /* read mode */
		switch (ui32mode) {
		case GED_GPU_INFO_CAPABILITY:
			ged_gpu_info_dump_cap(psSeqFile, bHumanReadable);
			break;
		default:
			ged_gpu_info_profile(psSeqFile, false);
			break;
		case GED_GPU_INFO_RUNTIME:
			ged_gpu_info_profile(psSeqFile, bHumanReadable);
			break;
		}

	} else { /* configure mode */
		if (i32Mode < 0) {
			ui32mode = -1 * i32Mode;
			bHumanReadable = true;
		} else {
			ui32mode = i32Mode;
			bHumanReadable = false;
		}
	}
}

static ssize_t ged_gpu_info_write_entry(
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
	char *pcCMD;
	char *pcValue;
	int value = 0;
	int i;

	if (uiCount >= GED_HAL_DEBUGFS_SIZE)
		goto normal_exit;

	if (ged_copy_from_user(acBuffer, pszBuffer, uiCount))
		goto normal_exit;

	for (i = 0; i < NUM_TOKEN; i++)
		aint32Indx[i] = 0;

	acBuffer[uiCount] = '\0';
	i = tokenizer(acBuffer, uiCount, aint32Indx, NUM_TOKEN);
	if (i != NUM_TOKEN)
		goto normal_exit;

	pcCMD = acBuffer + aint32Indx[0];
	pcValue = acBuffer + aint32Indx[1];

	i = kstrtoint(pcValue, 0, &value);

	if (strcmp(pcCMD, "query") == 0)
		ged_get_gpu_info(NULL, value);
	else
		GED_LOGE("Unknown command:%s %c", pcCMD, *pcValue);

normal_exit:
	return uiCount;
}

static void *ged_gpu_info_seq_start(struct seq_file *psSeqFile,
loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_gpu_info_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}

static void *ged_gpu_info_seq_next(struct seq_file *psSeqFile, void *pvData,
loff_t *puiPosition)
{
	return NULL;
}

static int ged_gpu_info_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL)
		ged_get_gpu_info(psSeqFile, -1);

	return 0;
}

static const struct seq_operations gsGPUInfoReadOps = {
	.start = ged_gpu_info_seq_start,
	.stop = ged_gpu_info_seq_stop,
	.next = ged_gpu_info_seq_next,
	.show = ged_gpu_info_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
#ifdef GED_DEBUG_FS
static ssize_t ged_dvfs_tuning_mode_write_entry(const char __user *pszBuffer,
size_t uiCount, loff_t uiPosition, void *pvData)
{
#define GED_HAL_DEBUGFS_SIZE 64
	char acBuffer[GED_HAL_DEBUGFS_SIZE];


	if ((uiCount > 0) && (uiCount < GED_HAL_DEBUGFS_SIZE)) {
		if (ged_copy_from_user(acBuffer, pszBuffer, uiCount) == 0) {
			GED_DVFS_TUNING_MODE eTuningMode;

			acBuffer[uiCount] = '\0';
			if (kstrtoint(acBuffer, 0, (int *)&eTuningMode) == 0) {
				if (eTuningMode >= GED_DVFS_DEFAULT &&
				eTuningMode <= GED_DVFS_PERFORMANCE)
					ged_dvfs_set_tuning_mode(eTuningMode);
			}
		}
	}

	return uiCount;
}

static void *ged_dvfs_tuning_mode_seq_start(struct seq_file *psSeqFile,
	loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;


	return NULL;
}

static void ged_dvfs_tuning_mode_seq_stop(struct seq_file *psSeqFile,
	void *pvData)
{

}

static void *ged_dvfs_tuning_mode_seq_next(struct seq_file *psSeqFile,
	void *pvData, loff_t *puiPosition)
{
	return NULL;
}


static int ged_dvfs_tuning_mode_seq_show(struct seq_file *psSeqFile,
	void *pvData)
{
	if (pvData != NULL) {
		GED_DVFS_TUNING_MODE eTuningMode;

		eTuningMode = ged_dvfs_get_tuning_mode();
		seq_printf(psSeqFile, "%u\n", eTuningMode);
	}

	return 0;
}

static const struct seq_operations gsDvfs_tuning_mode_ReadOps = {
	.start = ged_dvfs_tuning_mode_seq_start,
	.stop = ged_dvfs_tuning_mode_seq_stop,
	.next = ged_dvfs_tuning_mode_seq_next,
	.show = ged_dvfs_tuning_mode_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
#ifdef GED_DEBUG_FS
static void *ged_dvfs_cur_freq_seq_start(struct seq_file *psSeqFile,
	loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;


	return NULL;
}

static void ged_dvfs_cur_freq_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}

static void *ged_dvfs_cur_freq_seq_next(struct seq_file *psSeqFile,
	void *pvData, loff_t *puiPosition)
{
	return NULL;
}


static int ged_dvfs_cur_freq_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL) {
		struct GED_DVFS_FREQ_DATA sFreqInfo;

		ged_dvfs_get_gpu_cur_freq(&sFreqInfo);
		seq_printf(psSeqFile, "%u %lu\n",
			sFreqInfo.ui32Idx, sFreqInfo.ulFreq);
	}

	return 0;
}

static const struct seq_operations gsDvfs_cur_freq_ReadOps = {
	.start = ged_dvfs_cur_freq_seq_start,
	.stop = ged_dvfs_cur_freq_seq_stop,
	.next = ged_dvfs_cur_freq_seq_next,
	.show = ged_dvfs_cur_freq_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
static ssize_t current_freqency_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct GED_DVFS_FREQ_DATA sFreqInfo;

	ged_dvfs_get_gpu_cur_freq(&sFreqInfo);

	return scnprintf(buf, PAGE_SIZE, "%u %lu\n",
		sFreqInfo.ui32Idx, sFreqInfo.ulFreq);
}

static KOBJ_ATTR_RO(current_freqency);
//-----------------------------------------------------------------------------
#ifdef GED_DEBUG_FS
static void *ged_dvfs_pre_freq_seq_start(struct seq_file *psSeqFile,
	loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_dvfs_pre_freq_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}

static void *ged_dvfs_pre_freq_seq_next(struct seq_file *psSeqFile,
	void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_dvfs_pre_freq_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL) {
		struct GED_DVFS_FREQ_DATA sFreqInfo;

		ged_dvfs_get_gpu_pre_freq(&sFreqInfo);
		seq_printf(psSeqFile, "%u %lu\n", sFreqInfo.ui32Idx,
			sFreqInfo.ulFreq);
	}

	return 0;
}

static const struct seq_operations gsDvfs_pre_freq_ReadOps = {
	.start = ged_dvfs_pre_freq_seq_start,
	.stop = ged_dvfs_pre_freq_seq_stop,
	.next = ged_dvfs_pre_freq_seq_next,
	.show = ged_dvfs_pre_freq_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
static ssize_t previous_freqency_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct GED_DVFS_FREQ_DATA sFreqInfo;

	ged_dvfs_get_gpu_pre_freq(&sFreqInfo);

	return scnprintf(buf, PAGE_SIZE, "%u %lu\n",
		sFreqInfo.ui32Idx, sFreqInfo.ulFreq);
}

static KOBJ_ATTR_RO(previous_freqency);
//-----------------------------------------------------------------------------
#ifdef GED_DEBUG_FS
static void *ged_dvfs_gpu_util_seq_start(struct seq_file *psSeqFile,
	loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_dvfs_gpu_util_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}

static void *ged_dvfs_gpu_util_seq_next(struct seq_file *psSeqFile,
	void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_dvfs_gpu_util_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL) {
		unsigned int loading;
		unsigned int block;
		unsigned int idle;

		mtk_get_gpu_loading(&loading);
		mtk_get_gpu_block(&block);
		mtk_get_gpu_idle(&idle);
		seq_printf(psSeqFile, "%u %u %u\n", loading, block, idle);
	}

	return 0;
}

static const struct seq_operations gsDvfs_gpu_util_ReadOps = {
	.start = ged_dvfs_gpu_util_seq_start,
	.stop = ged_dvfs_gpu_util_seq_stop,
	.next = ged_dvfs_gpu_util_seq_next,
	.show = ged_dvfs_gpu_util_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
static ssize_t gpu_utilization_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int loading;
	unsigned int block;
	unsigned int idle;

	mtk_get_gpu_loading(&loading);
	mtk_get_gpu_block(&block);
	mtk_get_gpu_idle(&idle);

	return scnprintf(buf, PAGE_SIZE, "%u %u %u\n", loading, block, idle);
}

static KOBJ_ATTR_RO(gpu_utilization);
//-----------------------------------------------------------------------------
#ifdef GED_DEBUG_FS
static uint32_t _fps_upper_bound = 60;

static void *ged_fps_ub_seq_start(struct seq_file *seq, loff_t *pos)
{
	return *pos ? NULL : SEQ_START_TOKEN;
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
	GED_LOGD("@%s", __func__);
	seq_printf(seq, "%u\n", _fps_upper_bound);
	return 0;
}

static const struct seq_operations gs_fps_ub_read_ops = {
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
	unsigned long ulTmp = 0;

	if (ged_copy_from_user(str_num, pszBuffer, MAX_FPS_DIGITS) == 0) {
		int error;
		str_num[MAX_FPS_DIGITS] = 0;
		error = kstrtoul(str_num, 10, &ulTmp);
		if (error != 0)
			return uiCount;
		_fps_upper_bound = ulTmp;
		ged_dvfs_probe_signal(GED_FPS_CHANGE_SIGNAL_EVENT);
		GED_LOGD("FPS is set to %d", _fps_upper_bound);
	}

	return uiCount;
}
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
static int32_t _boost_level = -1;
#define MAX_BOOST_DIGITS 10
#ifdef GED_DEBUG_FS
static void *ged_dvfs_boost_level_seq_start(
	struct seq_file *psSeqFile, loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_dvfs_boost_level_seq_stop(
	struct seq_file *psSeqFile, void *pvData)
{

}

static void *ged_dvfs_boost_level_seq_next(
	struct seq_file *psSeqFile, void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_dvfs_boost_level_seq_show(
	struct seq_file *psSeqFile, void *pvData)
{
	seq_printf(psSeqFile, "%d\n", _boost_level);
	return 0;
}

static const struct seq_operations gsDvfs_boost_level_ReadOps = {
	.start = ged_dvfs_boost_level_seq_start,
	.stop = ged_dvfs_boost_level_seq_stop,
	.next = ged_dvfs_boost_level_seq_next,
	.show = ged_dvfs_boost_level_seq_show,
};

static ssize_t ged_boost_level_write(const char __user *pszBuffer,
	size_t uiCount, loff_t uiPosition, void *pvData)
{
	char str_num[MAX_BOOST_DIGITS];
	long val;

	if (uiCount > 0 && uiCount < MAX_BOOST_DIGITS) {
		if (ged_copy_from_user(str_num, pszBuffer, uiCount) == 0) {
			str_num[uiCount] = '\0';
			if (kstrtol(str_num, 10, &val) == 0)
				_boost_level = (int32_t)val;
		}
	}

	return uiCount;
}
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
static ssize_t gpu_boost_level_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", _boost_level);
}

static ssize_t gpu_boost_level_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char str_num[MAX_BOOST_DIGITS];
	long val;

	if (count > 0 && count < MAX_BOOST_DIGITS) {
		if (scnprintf(str_num, MAX_BOOST_DIGITS, "%s", buf)) {
			if (kstrtol(str_num, 10, &val) == 0)
				_boost_level = (int32_t)val;
		}
	}

	return count;
}

static KOBJ_ATTR_RW(gpu_boost_level);
//-----------------------------------------------------------------------------
int ged_dvfs_boost_value(void)
{
	return _boost_level;
}

//-----------------------------------------------------------------------------
#ifdef GED_DEBUG_FS
static void *ged_dvfs_integration_report_seq_start(struct seq_file *psSeqFile,
		loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_dvfs_integration_report_seq_stop(struct seq_file *psSeqFile,
	void *pvData)
{

}

static void *ged_dvfs_integration_report_seq_next(struct seq_file *psSeqFile,
	void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_dvfs_integration_report_seq_show(struct seq_file *psSeqFile,
	void *pvData)
{
	if (pvData != NULL) {
#ifdef GED_ENABLE_DVFS_LOADING_MODE
		seq_printf(psSeqFile, "GPU Utilization EX fp: %p\n",
			ged_dvfs_cal_gpu_utilization_ex_fp);
#else
		seq_printf(psSeqFile, "GPU Utilization fp: %p\n",
			ged_dvfs_cal_gpu_utilization_fp);
#endif
		seq_printf(psSeqFile, "GPU DVFS idx commit fp: %p\n",
			ged_dvfs_gpu_freq_commit_fp);
		seq_printf(psSeqFile, "GPU clock notify on: %d\n",
			ged_gpu_power_on_notified);
		seq_printf(psSeqFile, "GPU clock notify off: %d\n",
			ged_gpu_power_off_notified);
	}
	return 0;
}

static const struct seq_operations gsIntegrationReportReadOps = {
	.start = ged_dvfs_integration_report_seq_start,
	.stop = ged_dvfs_integration_report_seq_stop,
	.next = ged_dvfs_integration_report_seq_next,
	.show = ged_dvfs_integration_report_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
#ifdef MTK_GED_KPI
#ifdef GED_DEBUG_FS
static void *ged_kpi_info_seq_start(struct seq_file *psSeqFile,
	loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_kpi_info_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}

static void *ged_kpi_info_seq_next(struct seq_file *psSeqFile,
	void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_kpi_info_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL) {
		unsigned int fps;
		unsigned int cpu_time;
		unsigned int gpu_time;
		unsigned int response_time;
		unsigned int gpu_remained_time;
		unsigned int cpu_remained_time;
		unsigned int gpu_freq;

		fps = ged_kpi_get_cur_fps();
		cpu_time = ged_kpi_get_cur_avg_cpu_time();
		gpu_time = ged_kpi_get_cur_avg_gpu_time();
		response_time = ged_kpi_get_cur_avg_response_time();
		cpu_remained_time = ged_kpi_get_cur_avg_cpu_remained_time();
		gpu_remained_time = ged_kpi_get_cur_avg_gpu_remained_time();
		gpu_freq = ged_kpi_get_cur_avg_gpu_freq();

		seq_printf(psSeqFile, "%u,%u,%u,%u,%u,%u,%u\n",
			fps, cpu_time, gpu_time,
			response_time, cpu_remained_time,
			gpu_remained_time, gpu_freq);
	}
	return 0;
}

static const struct seq_operations gsKpi_info_ReadOps = {
	.start = ged_kpi_info_seq_start,
	.stop = ged_kpi_info_seq_stop,
	.next = ged_kpi_info_seq_next,
	.show = ged_kpi_info_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
static ssize_t ged_kpi_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	unsigned int fps;
	unsigned int cpu_time;
	unsigned int gpu_time;
	unsigned int response_time;
	unsigned int gpu_remained_time;
	unsigned int cpu_remained_time;
	unsigned int gpu_freq;

	fps = ged_kpi_get_cur_fps();
	cpu_time = ged_kpi_get_cur_avg_cpu_time();
	gpu_time = ged_kpi_get_cur_avg_gpu_time();
	response_time = ged_kpi_get_cur_avg_response_time();
	cpu_remained_time = ged_kpi_get_cur_avg_cpu_remained_time();
	gpu_remained_time = ged_kpi_get_cur_avg_gpu_remained_time();
	gpu_freq = ged_kpi_get_cur_avg_gpu_freq();

	return scnprintf(buf, PAGE_SIZE, "%u,%u,%u,%u,%u,%u,%u\n",
			fps, cpu_time, gpu_time, response_time,
			cpu_remained_time, gpu_remained_time, gpu_freq);
}

static KOBJ_ATTR_RO(ged_kpi);
#endif /* MTK_GED_KPI */
//-----------------------------------------------------------------------------
#if (defined(GED_ENABLE_FB_DVFS) && defined(GED_ENABLE_DYNAMIC_DVFS_MARGIN))
#ifdef GED_DEBUG_FS
static ssize_t ged_dvfs_margin_value_write_entry
(const char __user *pszBuffer, size_t uiCount, loff_t uiPosition, void *pvData)
{
#define GED_HAL_DEBUGFS_SIZE 64
	char acBuffer[GED_HAL_DEBUGFS_SIZE];

	int i32Value;

	if ((uiCount > 0) && (uiCount < GED_HAL_DEBUGFS_SIZE)) {
		if (ged_copy_from_user(acBuffer, pszBuffer, uiCount) == 0) {
			acBuffer[uiCount] = '\0';
			//if (sscanf(acBuffer, "%d", &i32Value) == 1)
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_dvfs_margin_value(i32Value);

			//else if (...) //for other commands
			//{
			//}
		}
	}

	return uiCount;
}

static void *ged_dvfs_margin_value_seq_start(struct seq_file *psSeqFile,
			loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_dvfs_margin_value_seq_stop(struct seq_file *psSeqFile,
			void *pvData)
{

}

static void *ged_dvfs_margin_value_seq_next(struct seq_file *psSeqFile,
			void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_dvfs_margin_value_seq_show(struct seq_file *psSeqFile,
			void *pvData)
{
	if (pvData != NULL) {
		int i32DvfsMarginValue;

		if (false == mtk_get_dvfs_margin_value(&i32DvfsMarginValue)) {
			i32DvfsMarginValue = 0;
			seq_puts(psSeqFile,
				"call mtk_get_dvfs_margin_value false\n");
		}
		seq_printf(psSeqFile, "%d\n", i32DvfsMarginValue);
	}

	return 0;
}

const struct seq_operations gsDvfsMarginValueReadOps = {
	.start = ged_dvfs_margin_value_seq_start,
	.stop = ged_dvfs_margin_value_seq_stop,
	.next = ged_dvfs_margin_value_seq_next,
	.show = ged_dvfs_margin_value_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
static ssize_t dvfs_margin_value_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i32DvfsMarginValue;
	char temp[GED_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	if (false == mtk_get_dvfs_margin_value(&i32DvfsMarginValue)) {
		i32DvfsMarginValue = 0;
		length = scnprintf(temp + pos, GED_SYSFS_MAX_BUFF_SIZE - pos,
				"call mtk_get_dvfs_margin_value false\n");
		pos += length;
	}
	length = scnprintf(temp + pos, GED_SYSFS_MAX_BUFF_SIZE - pos,
			"%d\n", i32DvfsMarginValue);
	pos += length;

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t dvfs_margin_value_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_dvfs_margin_value(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dvfs_margin_value);
#endif /* (defined(GED_ENABLE_FB_DVFS) && ...) */
//-----------------------------------------------------------------------------
#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
#ifdef GED_DEBUG_FS
static ssize_t ged_loading_base_dvfs_step_write_entry
(const char __user *pszBuffer, size_t uiCount, loff_t uiPosition, void *pvData)
{
#define GED_HAL_DEBUGFS_SIZE 64
	char acBuffer[GED_HAL_DEBUGFS_SIZE];

	int i32Value;

	if ((uiCount > 0) && (uiCount < GED_HAL_DEBUGFS_SIZE)) {
		if (ged_copy_from_user(acBuffer, pszBuffer, uiCount) == 0) {
			acBuffer[uiCount] = '\0';
			//if (sscanf(acBuffer, "%x", &i32Value) == 1)
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_loading_base_dvfs_step(i32Value);
			//else if (...) //for other commands
			//{
			//}
		}
	}

	return uiCount;
}

static void *ged_loading_base_dvfs_step_seq_start(struct seq_file *psSeqFile,
			loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_loading_base_dvfs_step_seq_stop(struct seq_file *psSeqFile,
			void *pvData)
{

}

static void *ged_loading_base_dvfs_step_seq_next(struct seq_file *psSeqFile,
			void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_loading_base_dvfs_step_seq_show(struct seq_file *psSeqFile,
			void *pvData)
{
	if (pvData != NULL) {
		int i32StepValue;

		if (false == mtk_get_loading_base_dvfs_step(&i32StepValue)) {
			i32StepValue = 0;
			seq_puts(psSeqFile,
				"call mtk_get_loading_base_dvfs_step false\n");
		}
		seq_printf(psSeqFile, "%x\n", i32StepValue);
	}

	return 0;
}

const struct seq_operations gsLoadingBaseDvfsStepReadOps = {
	.start = ged_loading_base_dvfs_step_seq_start,
	.stop = ged_loading_base_dvfs_step_seq_stop,
	.next = ged_loading_base_dvfs_step_seq_next,
	.show = ged_loading_base_dvfs_step_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
static ssize_t loading_base_dvfs_step_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i32StepValue;
	char temp[GED_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	if (false == mtk_get_loading_base_dvfs_step(&i32StepValue)) {
		i32StepValue = 0;
		length = scnprintf(temp + pos, GED_SYSFS_MAX_BUFF_SIZE - pos,
				"call mtk_get_loading_base_dvfs_step false\n");
		pos += length;
	}
	length = scnprintf(temp + pos, GED_SYSFS_MAX_BUFF_SIZE - pos,
			"%x\n", i32StepValue);
	pos += length;

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t loading_base_dvfs_step_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_loading_base_dvfs_step(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(loading_base_dvfs_step);
#endif /* GED_CONFIGURE_LOADING_BASE_DVFS_STEP */
//-----------------------------------------------------------------------------
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
#ifdef GED_DEBUG_FS
static ssize_t ged_timer_base_dvfs_margin_write_entry
(const char __user *pszBuffer, size_t uiCount, loff_t uiPosition, void *pvData)
{
#define GED_HAL_DEBUGFS_SIZE 64
	char acBuffer[GED_HAL_DEBUGFS_SIZE];

	int i32Value;

	if ((uiCount > 0) && (uiCount < GED_HAL_DEBUGFS_SIZE)) {
		if (ged_copy_from_user(acBuffer, pszBuffer, uiCount) == 0) {
			acBuffer[uiCount] = '\0';
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_timer_base_dvfs_margin(i32Value);
		}
	}

	return uiCount;
}

static void *ged_timer_base_dvfs_margin_seq_start(struct seq_file *psSeqFile,
			loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_timer_base_dvfs_margin_seq_stop(struct seq_file *psSeqFile,
			void *pvData)
{

}

static void *ged_timer_base_dvfs_margin_seq_next(struct seq_file *psSeqFile,
			void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_timer_base_dvfs_margin_seq_show(struct seq_file *psSeqFile,
			void *pvData)
{
	if (pvData != NULL) {
		int i32DvfsMarginValue;

	if (false == mtk_get_timer_base_dvfs_margin(&i32DvfsMarginValue)) {
		i32DvfsMarginValue = 0;
		seq_puts(psSeqFile, "call mtk_get_timer_base_dvfs_margin false\n");
	}
	seq_printf(psSeqFile, "%d\n", i32DvfsMarginValue);
	}

	return 0;
}

const struct seq_operations gsTimerBaseDvfsMarginReadOps = {
	.start = ged_timer_base_dvfs_margin_seq_start,
	.stop = ged_timer_base_dvfs_margin_seq_stop,
	.next = ged_timer_base_dvfs_margin_seq_next,
	.show = ged_timer_base_dvfs_margin_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
static ssize_t timer_base_dvfs_margin_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i32DvfsMarginValue;
	char temp[GED_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	if (false == mtk_get_timer_base_dvfs_margin(&i32DvfsMarginValue)) {
		i32DvfsMarginValue = 0;
		length = scnprintf(temp + pos, GED_SYSFS_MAX_BUFF_SIZE - pos,
				"call mtk_get_timer_base_dvfs_margin false\n");
		pos += length;
	}
	length = scnprintf(temp + pos, GED_SYSFS_MAX_BUFF_SIZE - pos,
			"%d\n", i32DvfsMarginValue);
	pos += length;

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t timer_base_dvfs_margin_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_timer_base_dvfs_margin(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(timer_base_dvfs_margin);
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */
//-----------------------------------------------------------------------------
#ifdef GED_ENABLE_DVFS_LOADING_MODE
#ifdef GED_DEBUG_FS
static ssize_t ged_dvfs_loading_mode_write_entry
(const char __user *pszBuffer, size_t uiCount, loff_t uiPosition, void *pvData)
{
#define GED_HAL_DEBUGFS_SIZE 64
	char acBuffer[GED_HAL_DEBUGFS_SIZE];

	int i32Value;

	if ((uiCount > 0) && (uiCount < GED_HAL_DEBUGFS_SIZE)) {
		if (ged_copy_from_user(acBuffer, pszBuffer, uiCount) == 0) {
			acBuffer[uiCount] = '\0';
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_dvfs_loading_mode(i32Value);
		}
	}

	return uiCount;
}

static void *ged_dvfs_loading_mode_seq_start(struct seq_file *psSeqFile,
			loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_dvfs_loading_mode_seq_stop(struct seq_file *psSeqFile,
			void *pvData)
{

}

static void *ged_dvfs_loading_mode_seq_next(struct seq_file *psSeqFile,
			void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_dvfs_loading_mode_seq_show(struct seq_file *psSeqFile,
			void *pvData)
{
	if (pvData != NULL) {
		unsigned int ui32DvfsLoadingMode;

		if (false == mtk_get_dvfs_loading_mode(&ui32DvfsLoadingMode)) {
			ui32DvfsLoadingMode = 0;
			seq_puts(psSeqFile, "call mtk_get_dvfs_loading_mode false\n");
		}
		seq_printf(psSeqFile, "%d\n", ui32DvfsLoadingMode);
	}

	return 0;
}

const struct seq_operations gsDvfsLoadingModeReadOps = {
	.start = ged_dvfs_loading_mode_seq_start,
	.stop = ged_dvfs_loading_mode_seq_stop,
	.next = ged_dvfs_loading_mode_seq_next,
	.show = ged_dvfs_loading_mode_seq_show,
};
#endif /* GED_DEBUG_FS */
//-----------------------------------------------------------------------------
static ssize_t dvfs_loading_mode_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32DvfsLoadingMode;
	char temp[GED_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	if (false == mtk_get_dvfs_loading_mode(&ui32DvfsLoadingMode)) {
		ui32DvfsLoadingMode = 0;
		length = scnprintf(temp + pos, GED_SYSFS_MAX_BUFF_SIZE - pos,
				"call mtk_get_dvfs_loading_mode false\n");
		pos += length;
	}
	length = scnprintf(temp + pos, GED_SYSFS_MAX_BUFF_SIZE - pos,
			"%d\n", ui32DvfsLoadingMode);
	pos += length;

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t dvfs_loading_mode_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_dvfs_loading_mode(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dvfs_loading_mode);
#endif /* GED_ENABLE_DVFS_LOADING_MODE */

//-----------------------------------------------------------------------------
static struct notifier_block ged_fb_notifier;

static int ged_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		g_ui32EventStatus |= GED_EVENT_LCD;
		ged_dvfs_probe_signal(GED_GAS_SIGNAL_EVENT);
		break;
	case FB_BLANK_POWERDOWN:
		g_ui32EventStatus &= ~GED_EVENT_LCD;
		ged_dvfs_probe_signal(GED_GAS_SIGNAL_EVENT);
		break;
	default:
		break;
	}

	return 0;
}

struct ged_event_change_entry_t {
	ged_event_change_fp callback;
	void *private_data;
	char name[128];
	struct list_head sList;
};

static struct {
	struct mutex lock;
	struct list_head listen;
} g_ged_event_change = {
	.lock     = __MUTEX_INITIALIZER(g_ged_event_change.lock),
	.listen   = LIST_HEAD_INIT(g_ged_event_change.listen),
};

bool mtk_register_ged_event_change(const char *name,
	ged_event_change_fp callback, void *private_data)
{
	struct ged_event_change_entry_t *entry = NULL;

	entry = kmalloc(sizeof(struct ged_event_change_entry_t), GFP_KERNEL);
	if (entry == NULL)
		return false;

	entry->callback = callback;
	entry->private_data = private_data;
	strncpy(entry->name, name, sizeof(entry->name) - 1);
	entry->name[sizeof(entry->name) - 1] = 0;
	INIT_LIST_HEAD(&entry->sList);

	mutex_lock(&g_ged_event_change.lock);

	list_add(&entry->sList, &g_ged_event_change.listen);

	mutex_unlock(&g_ged_event_change.lock);

	return true;
}

bool mtk_unregister_ged_event_change(const char *name)
{
	struct list_head *pos, *head;
	struct ged_event_change_entry_t *entry = NULL;

	mutex_lock(&g_ged_event_change.lock);

	head = &g_ged_event_change.listen;
	list_for_each(pos, head) {
		entry = list_entry(pos, struct ged_event_change_entry_t, sList);
		if (strncmp(entry->name, name, sizeof(entry->name) - 1) == 0)
			break;
		entry = NULL;
	}

	if (entry) {
		list_del(&entry->sList);
		kfree(entry);
	}

	mutex_unlock(&g_ged_event_change.lock);

	return true;
}

void mtk_ged_event_notify(int events)
{
	struct list_head *pos, *head;
	struct ged_event_change_entry_t *entry = NULL;

	mutex_lock(&g_ged_event_change.lock);

	head = &g_ged_event_change.listen;
	list_for_each(pos, head) {
		entry = list_entry(pos, struct ged_event_change_entry_t, sList);
		entry->callback(entry->private_data, events);
	}

	mutex_unlock(&g_ged_event_change.lock);
}

//-----------------------------------------------------------------------------
GED_ERROR ged_hal_init(void)
{
	GED_ERROR err = GED_OK;

#ifdef GED_DEBUG_FS
	err = ged_debugFS_create_entry_dir(
			"hal",
			NULL,
			&gpsHALDir);

	if (unlikely(err != GED_OK)) {
		err = GED_ERROR_FAIL;
		GED_LOGE("Failed to create hal dir!\n");
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

	if (unlikely(err != GED_OK)) {
		GED_LOGE(
		"Failed to create total_gpu_freq_level_count entry!\n");
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

	if (unlikely(err != GED_OK)) {
		GED_LOGE(
		"Failed to create custom_boost_gpu_freq entry!\n");
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

	if (unlikely(err != GED_OK)) {
		GED_LOGE(
		"Failed to create custom_upbound_gpu_freq entry!\n");
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

	err = ged_debugFS_create_entry(
			"media_event",
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

	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create vsync_offset_level entry!\n");
		goto ERROR;
	}

	/* Game SDK */
	err = ged_debugFS_create_entry(
			"gpu_info",
			gpsHALDir,
			&gsGPUInfoReadOps,
			ged_gpu_info_write_entry,
			NULL,
			&gpsGPUInfoEntry);

	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_info entry!\n");
		goto ERROR;
	}

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
			&gpsFpsUpperBoundEntry);

	/* Get GPU boost level */
	err = ged_debugFS_create_entry(
			"gpu_boost_level",
			gpsHALDir,
			&gsDvfs_boost_level_ReadOps,
			ged_boost_level_write,
			NULL,
			&gpsBoostLevelEntry);

	/* Get KPI info */
#ifdef MTK_GED_KPI
	err = ged_debugFS_create_entry(
			"ged_kpi",
			gpsHALDir,
			&gsKpi_info_ReadOps,
			NULL,
			NULL,
			&gpsGedInfoKPIEntry);
#endif

#if (defined(GED_ENABLE_FB_DVFS) && defined(GED_ENABLE_DYNAMIC_DVFS_MARGIN))
	/* Control the gpu freq margin mode */
	err = ged_debugFS_create_entry(
			"dvfs_margin_value",
			gpsHALDir,
			&gsDvfsMarginValueReadOps,
			ged_dvfs_margin_value_write_entry,
			NULL,
			&gpsDvfsMarginValueEntry);

	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dvfs_margin_value entry!\n");
		goto ERROR;
	}
#endif

#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
	/* Control the gpu freq margin mode */
	err = ged_debugFS_create_entry(
			"loading_base_dvfs_step",
			gpsHALDir,
			&gsLoadingBaseDvfsStepReadOps,
			ged_loading_base_dvfs_step_write_entry,
			NULL,
			&gpsLoadingBaseDvfsStepEntry);

	if (unlikely(err != GED_OK)) {
		GED_LOGE(
		"Failed to create loading_base_dvfs_step entry!\n");
		goto ERROR;
	}
#endif

#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
	/* Control the timer base gpu freq margin */
	err = ged_debugFS_create_entry(
			"timer_base_dvfs_margin",
			gpsHALDir,
			&gsTimerBaseDvfsMarginReadOps,
			ged_timer_base_dvfs_margin_write_entry,
			NULL,
			&gpsTimerBaseDvfsMarginEntry);

	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create tb dvfs_margin entry!\n");
		goto ERROR;
	}
#endif

#ifdef GED_ENABLE_DVFS_LOADING_MODE
	/* Control the gpu loading mode */
	err = ged_debugFS_create_entry(
		"dvfs_loading_mode",
		gpsHALDir,
		&gsDvfsLoadingModeReadOps,
		ged_dvfs_loading_mode_write_entry,
		NULL,
		&gpsDvfsLoadingModeEntry);

	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu loading mode entry!\n");
		goto ERROR;
	}
#endif

	/* Report Integration Status */
	err = ged_debugFS_create_entry(
			"integration_report",
			gpsHALDir,
			&gsIntegrationReportReadOps,
			NULL,
			NULL,
			&gpsIntegrationReportReadEntry);

	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create integration_report entry!\n");
		goto ERROR;
	}
#endif /* GED_DEBUG_FS */

	err = ged_sysfs_create_dir(NULL, "hal", &hal_kobj);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create hal dir!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_total_gpu_freq_level_count);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create total_gpu_freq_level_count entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_custom_boost_gpu_freq);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create custom_boost_gpu_freq entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_custom_upbound_gpu_freq);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create custom_upbound_gpu_freq entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_current_freqency);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create current_freqency entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_previous_freqency);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create previous_freqency entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_utilization);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_utilization entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_boost_level);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_boost_level entry!\n");
		goto ERROR;
	}

#ifdef MTK_GED_KPI
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_ged_kpi);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create ged_kpi entry!\n");
		goto ERROR;
	}
#endif

#if (defined(GED_ENABLE_FB_DVFS) && defined(GED_ENABLE_DYNAMIC_DVFS_MARGIN))
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dvfs_margin_value);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dvfs_margin_value entry!\n");
		goto ERROR;
	}
#endif

#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_loading_base_dvfs_step);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create loading_base_dvfs_step entry!\n");
		goto ERROR;
	}
#endif

#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_timer_base_dvfs_margin);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create timer_base_dvfs_margin entry!\n");
		goto ERROR;
	}
#endif

#ifdef GED_ENABLE_DVFS_LOADING_MODE
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dvfs_loading_mode);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dvfs_loading_mode entry!\n");
		goto ERROR;
	}
#endif

	ged_fb_notifier.notifier_call = ged_fb_notifier_callback;
	if (fb_register_client(&ged_fb_notifier))
		GED_LOGE("Register fb_notifier fail!\n");

	return err;

ERROR:

	ged_hal_exit();

	return err;
}
//-----------------------------------------------------------------------------
void ged_hal_exit(void)
{
#ifdef GED_DEBUG_FS
	ged_debugFS_remove_entry(gpsIntegrationReportReadEntry);
#ifdef GED_ENABLE_DVFS_LOADING_MODE
	ged_debugFS_remove_entry(gpsDvfsLoadingModeEntry);
#endif
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
	ged_debugFS_remove_entry(gpsTimerBaseDvfsMarginEntry);
#endif
#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
	ged_debugFS_remove_entry(gpsLoadingBaseDvfsStepEntry);
#endif
#if (defined(GED_ENABLE_FB_DVFS) && defined(GED_ENABLE_DYNAMIC_DVFS_MARGIN))
	ged_debugFS_remove_entry(gpsDvfsMarginValueEntry);
#endif
#ifdef MTK_GED_KPI
	ged_debugFS_remove_entry(gpsGedInfoKPIEntry);
#endif
	ged_debugFS_remove_entry(gpsBoostLevelEntry);
	ged_debugFS_remove_entry(gpsFpsUpperBoundEntry);
	ged_debugFS_remove_entry(gpsDvfsGpuUtilizationEntry);
	ged_debugFS_remove_entry(gpsDvfsPreFreqEntry);
	ged_debugFS_remove_entry(gpsDvfsCurFreqEntry);
	ged_debugFS_remove_entry(gpsDvfsTuningModeEntry);
	ged_debugFS_remove_entry(gpsGPUInfoEntry);
	ged_debugFS_remove_entry(gpsVsyncOffsetLevelEntry);
	ged_debugFS_remove_entry(gpsVsyncOffsetEnableEntry);
	ged_debugFS_remove_entry(gpsCustomUpboundGPUFreqEntry);
	ged_debugFS_remove_entry(gpsCustomBoostGPUFreqEntry);
	ged_debugFS_remove_entry(gpsTotalGPUFreqLevelCountEntry);
	ged_debugFS_remove_entry_dir(gpsHALDir);
#endif /* GED_DEBUG_FS */

#ifdef GED_ENABLE_DVFS_LOADING_MODE
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dvfs_loading_mode);
#endif
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_timer_base_dvfs_margin);
#endif
#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_loading_base_dvfs_step);
#endif
#if (defined(GED_ENABLE_FB_DVFS) && defined(GED_ENABLE_DYNAMIC_DVFS_MARGIN))
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dvfs_margin_value);
#endif
#ifdef MTK_GED_KPI
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_ged_kpi);
#endif
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_boost_level);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_utilization);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_previous_freqency);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_current_freqency);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_custom_upbound_gpu_freq);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_custom_boost_gpu_freq);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_total_gpu_freq_level_count);
	ged_sysfs_remove_dir(&hal_kobj);
}
//-----------------------------------------------------------------------------
