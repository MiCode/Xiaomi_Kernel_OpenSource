/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2017-2018 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (C) 2018-2019 Ian Su <ian.su@tw.synaptics.com>
 * Copyright (C) 2018-2019 Joey Zhou <joey.zhou@synaptics.com>
 * Copyright (C) 2018-2019 Yuehao Qiu <yuehao.qiu@synaptics.com>
 * Copyright (C) 2018-2019 Aaron Chen <aaron.chen@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>
#include "synaptics_tcm_core.h"

/* #define RESET_ON_RESUME */

/* #define RESUME_EARLY_UNBLANK */

#define RESET_ON_RESUME_DELAY_MS 50

#define PREDICTIVE_READING

#define MIN_READ_LENGTH 9

/* #define FORCE_RUN_APPLICATION_FIRMWARE */

#define NOTIFIER_PRIORITY 2

#define RESPONSE_TIMEOUT_MS 3000

#define APP_STATUS_POLL_TIMEOUT_MS 1000

#define APP_STATUS_POLL_MS 100

#define ENABLE_IRQ_DELAY_MS 20

#define FALL_BACK_ON_POLLING

#define POLLING_DELAY_MS 5

#define MODE_SWITCH_DELAY_MS 100

#define READ_RETRY_US_MIN 5000

#define READ_RETRY_US_MAX 10000

#define WRITE_DELAY_US_MIN 500

#define WRITE_DELAY_US_MAX 1000

#define DYNAMIC_CONFIG_SYSFS_DIR_NAME "dynamic_config"

#define ROMBOOT_DOWNLOAD_UNIT 16

#define PDT_END_ADDR 0x00ee

#define RMI_UBL_FN_NUMBER 0x35

#define K9E_ID_DET (336+96)

/* #define GRIP_MODE_DEBUG */

#define SYNA_GAME_MODE_ARRAY		"synaptics,game-mode-array"
#define SYNA_ACTIVE_MODE_ARRAY		"synaptics,active-mode-array"
#define SYNA_UP_THRESHOLD_ARRAY 	"synaptics,up-threshold-array"
#define SYNA_TOLERANCE_ARRAY		"synaptics,tolerance-array"
#define SYNA_EDGE_FILTER_ARRAY		"synaptics,edge-filter-array"
#define SYNA_PANEL_ORIEN_ARRAY		"synaptics,panel-orien-array"
#define SYNA_REPORT_RATE_ARRAY		"synaptics,report-rate-array"
#define SYNA_CORNER_FILTER_AREA_STEP_ARRAY  "synaptics,cornerfilter-area-step-array"
#define SYNA_CORNER_ZONE_FILTER_HOR1_ARRAY  "synaptics,cornerzone-filter-hor1-array"
#define SYNA_CORNER_ZONE_FILTER_HOR2_ARRAY  "synaptics,cornerzone-filter-hor2-array"
#define SYNA_CORNER_ZONE_FILTER_VER_ARRAY   "synaptics,cornerzone-filter-ver-array"
#define SYNA_DEAD_ZONE_FILTER_HOR_ARRAY     "synaptics,deadzone-filter-hor-array"
#define SYNA_DEAD_ZONE_FILTER_VER_ARRAY     "synaptics,deadzone-filter-ver-array"
#define SYNA_EDGE_ZONE_FILTER_HOR_ARRAY     "synaptics,edgezone-filter-hor-array"
#define SYNA_EDGE_ZONE_FILTER_VER_ARRAY     "synaptics,edgezone-filter-ver-array"
#define SYNA_DISPLAY_RESOLUTION_ARRAY       "synaptics,panel-display-resolution"

enum  syna_dts_index{
	SYNA_DTS_GET_MAX_INDEX = 0,
	SYNA_DTS_GET_MIN_INDEX,
	SYNA_DTS_GET_DEF_INDEX,
	SYNA_DTS_SET_CUR_INDEX,
	SYNA_DTS_GET_CUR_INDEX,
};

static struct syna_tcm_hcd *gloab_tcm_hcd;
static bool tp_probe_success;
static void syna_tcm_reinit_mode(void);
static int syna_tcm_palm_area_change_setting(int value);

static const unsigned int touch_mode_dc_id_table[][2] = {
/*	{Touch_Active_MODE,       DC_NO_DOZE}, */
	{Touch_UP_THRESHOLD,      DC_FAST_TAP_HYTERESIS},
	{Touch_Tolerance,         DC_MOTION_TOLERANCE},
	{Touch_Report_Rate,       DC_SET_REPORT_RATE},
/*	{Touch_Aim_Sensitivity    DC_UNKNOWN}, */
	{Touch_Mode_NUM,          DC_UNKNOWN}
};

static unsigned int syna_tcm_get_dc_id(unsigned int mode)
{
	unsigned int i = 0;

	while (touch_mode_dc_id_table[i][0] < Touch_Mode_NUM) {
		if (touch_mode_dc_id_table[i][0] == mode) {
			break;
		}

		i++;
	}

	return touch_mode_dc_id_table[i][1];
}

#if (USE_KOBJ_SYSFS)

#define dynamic_config_sysfs(c_name, id) \
static ssize_t syna_tcm_sysfs_##c_name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, char *buf) \
{ \
	int retval; \
	unsigned short value; \
	struct device *p_dev; \
	struct kobject *p_kobj; \
	struct syna_tcm_hcd *tcm_hcd; \
\
	p_kobj = sysfs_dir->parent; \
	p_dev = container_of(p_kobj, struct device, kobj); \
	tcm_hcd = dev_get_drvdata(p_dev); \
\
	mutex_lock(&tcm_hcd->extif_mutex); \
\
	retval = tcm_hcd->get_dynamic_config(tcm_hcd, id, &value); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to get dynamic config\n"); \
		goto exit; \
	} \
\
	retval = snprintf(buf, PAGE_SIZE, "%u\n", value); \
\
exit: \
	mutex_unlock(&tcm_hcd->extif_mutex); \
\
	return retval; \
} \
\
static ssize_t syna_tcm_sysfs_##c_name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, const char *buf, size_t count) \
{ \
	int retval; \
	unsigned int input; \
	struct device *p_dev; \
	struct kobject *p_kobj; \
	struct syna_tcm_hcd *tcm_hcd; \
\
	p_kobj = sysfs_dir->parent; \
	p_dev = container_of(p_kobj, struct device, kobj); \
	tcm_hcd = dev_get_drvdata(p_dev); \
\
	if (sscanf(buf, "%u", &input) != 1) \
		return -EINVAL; \
\
	mutex_lock(&tcm_hcd->extif_mutex); \
\
	retval = tcm_hcd->set_dynamic_config(tcm_hcd, id, input); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to set dynamic config\n"); \
		goto exit; \
	} \
\
	retval = count; \
\
exit: \
	mutex_unlock(&tcm_hcd->extif_mutex); \
\
	return retval; \
}

#else

#define dynamic_config_sysfs(c_name, id) \
static ssize_t syna_tcm_sysfs_##c_name##_show(struct device *dev, \
		struct device_attribute *attr, char *buf) \
{ \
	int retval; \
	unsigned short value; \
	struct device *p_dev; \
	struct kobject *p_kobj; \
	struct syna_tcm_hcd *tcm_hcd; \
\
	p_kobj = sysfs_dir->parent; \
	p_dev = container_of(p_kobj, struct device, kobj); \
	tcm_hcd = dev_get_drvdata(p_dev); \
\
	mutex_lock(&tcm_hcd->extif_mutex); \
\
	retval = tcm_hcd->get_dynamic_config(tcm_hcd, id, &value); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to get dynamic config\n"); \
		goto exit; \
	} \
\
	retval = snprintf(buf, PAGE_SIZE, "%u\n", value); \
\
exit: \
	mutex_unlock(&tcm_hcd->extif_mutex); \
\
	return retval; \
} \
\
static ssize_t syna_tcm_sysfs_##c_name##_store(struct device *dev, \
		struct device_attribute *attr, const char *buf, size_t count) \
{ \
	int retval; \
	unsigned int input; \
	struct device *p_dev; \
	struct kobject *p_kobj; \
	struct syna_tcm_hcd *tcm_hcd; \
\
	p_kobj = sysfs_dir->parent; \
	p_dev = container_of(p_kobj, struct device, kobj); \
	tcm_hcd = dev_get_drvdata(p_dev); \
\
	if (sscanf(buf, "%u", &input) != 1) \
		return -EINVAL; \
\
	mutex_lock(&tcm_hcd->extif_mutex); \
\
	retval = tcm_hcd->set_dynamic_config(tcm_hcd, id, input); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to set dynamic config\n"); \
		goto exit; \
	} \
\
	retval = count; \
\
exit: \
	mutex_unlock(&tcm_hcd->extif_mutex); \
\
	return retval; \
}
#endif

DECLARE_COMPLETION(response_complete);

static struct kobject *sysfs_dir;

static struct syna_tcm_module_pool mod_pool;

#if (USE_KOBJ_SYSFS)
KOBJ_SHOW_PROTOTYPE(syna_tcm, info)
KOBJ_SHOW_PROTOTYPE(syna_tcm, info_appfw)
KOBJ_STORE_PROTOTYPE(syna_tcm, irq_en)
KOBJ_STORE_PROTOTYPE(syna_tcm, reset)
KOBJ_STORE_PROTOTYPE(syna_tcm, cb_debug)
KOBJ_STORE_PROTOTYPE(syna_tcm, misc_debug)
#ifdef WATCHDOG_SW
KOBJ_STORE_PROTOTYPE(syna_tcm, watchdog)
#endif
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, no_doze)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, disable_noise_mitigation)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, inhibit_frequency_shift)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, requested_frequency)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, disable_hsync)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, rezero_on_exit_deep_sleep)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, charger_connected)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, no_baseline_relaxation)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, in_wakeup_gesture_mode)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, stimulus_fingers)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, grip_suppression_enabled)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, enable_thick_glove)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, enable_glove)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, enable_touch_and_hold)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, game_mode_ctrl)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, set_report_rate)
KOBJ_SHOW_STORE_PROTOTYPE(syna_tcm, enable_gesture_type)

static struct kobj_attribute *attrs[] = {
	KOBJ_ATTRIFY(info),
	KOBJ_ATTRIFY(info_appfw),
	KOBJ_ATTRIFY(irq_en),
	KOBJ_ATTRIFY(reset),
	KOBJ_ATTRIFY(cb_debug),
	KOBJ_ATTRIFY(misc_debug),
#ifdef WATCHDOG_SW
	KOBJ_ATTRIFY(watchdog),
#endif
};

static struct kobj_attribute *dynamic_config_attrs[] = {
	KOBJ_ATTRIFY(no_doze),
	KOBJ_ATTRIFY(disable_noise_mitigation),
	KOBJ_ATTRIFY(inhibit_frequency_shift),
	KOBJ_ATTRIFY(requested_frequency),
	KOBJ_ATTRIFY(disable_hsync),
	KOBJ_ATTRIFY(rezero_on_exit_deep_sleep),
	KOBJ_ATTRIFY(charger_connected),
	KOBJ_ATTRIFY(no_baseline_relaxation),
	KOBJ_ATTRIFY(in_wakeup_gesture_mode),
	KOBJ_ATTRIFY(stimulus_fingers),
	KOBJ_ATTRIFY(grip_suppression_enabled),
	KOBJ_ATTRIFY(enable_thick_glove),
	KOBJ_ATTRIFY(enable_glove),
	KOBJ_ATTRIFY(enable_touch_and_hold),
	KOBJ_ATTRIFY(game_mode_ctrl),
	KOBJ_ATTRIFY(set_report_rate),
	KOBJ_ATTRIFY(enable_gesture_type),
};

#else /* apply device attribute declarations */

SHOW_PROTOTYPE(syna_tcm, info)
SHOW_PROTOTYPE(syna_tcm, info_appfw)
STORE_PROTOTYPE(syna_tcm, irq_en)
STORE_PROTOTYPE(syna_tcm, reset)
#ifdef WATCHDOG_SW
STORE_PROTOTYPE(syna_tcm, watchdog)
#endif
SHOW_STORE_PROTOTYPE(syna_tcm, no_doze)
SHOW_STORE_PROTOTYPE(syna_tcm, disable_noise_mitigation)
SHOW_STORE_PROTOTYPE(syna_tcm, inhibit_frequency_shift)
SHOW_STORE_PROTOTYPE(syna_tcm, requested_frequency)
SHOW_STORE_PROTOTYPE(syna_tcm, disable_hsync)
SHOW_STORE_PROTOTYPE(syna_tcm, rezero_on_exit_deep_sleep)
SHOW_STORE_PROTOTYPE(syna_tcm, charger_connected)
SHOW_STORE_PROTOTYPE(syna_tcm, no_baseline_relaxation)
SHOW_STORE_PROTOTYPE(syna_tcm, in_wakeup_gesture_mode)
SHOW_STORE_PROTOTYPE(syna_tcm, stimulus_fingers)
SHOW_STORE_PROTOTYPE(syna_tcm, grip_suppression_enabled)
SHOW_STORE_PROTOTYPE(syna_tcm, enable_thick_glove)
SHOW_STORE_PROTOTYPE(syna_tcm, enable_glove)
SHOW_STORE_PROTOTYPE(syna_tcm, enable_touch_and_hold)
SHOW_STORE_PROTOTYPE(syna_tcm, game_mode_ctrl)
SHOW_STORE_PROTOTYPE(syna_tcm, set_report_rate)
SHOW_STORE_PROTOTYPE(syna_tcm, enable_gesture_type)

static struct device_attribute *attrs[] = {
	ATTRIFY(info),
	ATTRIFY(info_appfw),
	ATTRIFY(irq_en),
	ATTRIFY(reset),
#ifdef WATCHDOG_SW
	ATTRIFY(watchdog),
#endif
};

static struct device_attribute *dynamic_config_attrs[] = {
	ATTRIFY(no_doze),
	ATTRIFY(disable_noise_mitigation),
	ATTRIFY(inhibit_frequency_shift),
	ATTRIFY(requested_frequency),
	ATTRIFY(disable_hsync),
	ATTRIFY(rezero_on_exit_deep_sleep),
	ATTRIFY(charger_connected),
	ATTRIFY(no_baseline_relaxation),
	ATTRIFY(in_wakeup_gesture_mode),
	ATTRIFY(stimulus_fingers),
	ATTRIFY(grip_suppression_enabled),
	ATTRIFY(enable_thick_glove),
	ATTRIFY(enable_glove),
	ATTRIFY(enable_touch_and_hold),
	ATTRIFY(game_mode_ctrl),
	ATTRIFY(set_report_rate),
	ATTRIFY(enable_gesture_type),
};
#endif /* END of #if USE_KOBJ_SYSFS*/

static int syna_tcm_get_app_info(struct syna_tcm_hcd *tcm_hcd);
static int syna_tcm_sensor_detection(struct syna_tcm_hcd *tcm_hcd);
static void syna_tcm_check_hdl(struct syna_tcm_hcd *tcm_hcd,
							unsigned char id);

#if (USE_KOBJ_SYSFS)
static ssize_t syna_tcm_sysfs_info_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
#else
static ssize_t syna_tcm_sysfs_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
#endif
{
	int retval;
	unsigned int count;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm_hcd *tcm_hcd;

	p_kobj = sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm_hcd = dev_get_drvdata(p_dev);

	LOGE(tcm_hcd->pdev->dev.parent,
				"PAGE_SIZE = 0x%lx\n",
				PAGE_SIZE);

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = tcm_hcd->identify(tcm_hcd, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification\n");
		goto exit;
	}

	count = 0;
	retval = snprintf(buf, PAGE_SIZE - count,
			"TouchComm version:  %d\n",
			tcm_hcd->id_info.version);
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	if (SYNAPTICS_TCM_ID_SUBVERSION == 0) {
		retval = snprintf(buf, PAGE_SIZE - count,
				"Driver version:     %d.%d\n",
				(unsigned char)(SYNAPTICS_TCM_ID_VERSION >> 8),
				(unsigned char)SYNAPTICS_TCM_ID_VERSION);
	} else {
		retval = snprintf(buf, PAGE_SIZE - count,
				"Driver version:     %d.%d.%d\n",
				(unsigned char)(SYNAPTICS_TCM_ID_VERSION >> 8),
				(unsigned char)SYNAPTICS_TCM_ID_VERSION,
				SYNAPTICS_TCM_ID_SUBVERSION);
	}
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	switch (tcm_hcd->id_info.mode) {
	case MODE_APPLICATION_FIRMWARE:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Application Firmware\n");
		if (retval < 0)
			goto exit;
		break;
	case MODE_HOSTDOWNLOAD_FIRMWARE:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Host Download Firmware\n");
		if (retval < 0)
			goto exit;
		break;
	case MODE_BOOTLOADER:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Bootloader\n");
		if (retval < 0)
			goto exit;
		break;
	case MODE_TDDI_BOOTLOADER:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      TDDI Bootloader\n");
		if (retval < 0)
			goto exit;
		break;
	case MODE_TDDI_HOSTDOWNLOAD_BOOTLOADER:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      TDDI Host Download Bootloader\n");
		if (retval < 0)
			goto exit;
		break;
	case MODE_ROMBOOTLOADER:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Rom Bootloader\n");
		if (retval < 0)
			goto exit;
		break;
	default:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Unknown (%d)\n",
				tcm_hcd->id_info.mode);
		if (retval < 0)
			goto exit;
		break;
	}
	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
			"Part number:        ");
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = secure_memcpy(buf,
			PAGE_SIZE - count,
			tcm_hcd->id_info.part_number,
			sizeof(tcm_hcd->id_info.part_number),
			sizeof(tcm_hcd->id_info.part_number));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy part number string\n");
		goto exit;
	}
	buf += sizeof(tcm_hcd->id_info.part_number);
	count += sizeof(tcm_hcd->id_info.part_number);

	retval = snprintf(buf, PAGE_SIZE - count,
			"\n");
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
			"Packrat number:     %d\n",
			tcm_hcd->packrat_number);
	if (retval < 0)
		goto exit;

	count += retval;

	retval = count;

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

#if (USE_KOBJ_SYSFS)
static ssize_t syna_tcm_sysfs_info_appfw_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
#else
static ssize_t syna_tcm_sysfs_info_appfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
#endif
{
	int retval;
	unsigned int count;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm_hcd *tcm_hcd;
	int i;

	p_kobj = sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm_hcd = dev_get_drvdata(p_dev);

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = syna_tcm_get_app_info(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get app info\n");
		goto exit;
	}

	count = 0;

	retval = snprintf(buf, PAGE_SIZE - count,
		"app info version:  %d\n",
		le2_to_uint(tcm_hcd->app_info.version));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"app info status:  %d\n",
		le2_to_uint(tcm_hcd->app_info.status));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"static config size:  %d\n",
		le2_to_uint(tcm_hcd->app_info.static_config_size));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"dynamic config size:  %d\n",
		le2_to_uint(tcm_hcd->app_info.dynamic_config_size));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"app config block:  %d\n",
		le2_to_uint(tcm_hcd->app_info.app_config_start_write_block));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"app config size:  %d\n",
		le2_to_uint(tcm_hcd->app_info.app_config_size));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"touch report config max size:  %d\n",
		le2_to_uint(tcm_hcd->app_info.max_touch_report_config_size));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"touch report payload max size:  %d\n",
		le2_to_uint(tcm_hcd->app_info.max_touch_report_payload_size));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count, "config id:  ");
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	for (i = 0; i < sizeof(tcm_hcd->app_info.customer_config_id); i++) {
		retval = snprintf(buf, PAGE_SIZE - count,
			"0x%2x ", tcm_hcd->app_info.customer_config_id[i]);
		buf += retval;
		count += retval;
	}

	retval = snprintf(buf, PAGE_SIZE - count, "\n");
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"max x:  %d\n",
		le2_to_uint(tcm_hcd->app_info.max_x));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"max y:  %d\n",
		le2_to_uint(tcm_hcd->app_info.max_y));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"max objects:  %d\n",
		le2_to_uint(tcm_hcd->app_info.max_objects));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"num cols:  %d\n",
		le2_to_uint(tcm_hcd->app_info.num_of_image_cols));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"num rows:  %d\n",
		le2_to_uint(tcm_hcd->app_info.num_of_image_rows));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"num buttons:  %d\n",
		le2_to_uint(tcm_hcd->app_info.num_of_buttons));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"has profile:  %d\n",
		le2_to_uint(tcm_hcd->app_info.has_hybrid_data));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"num force electrodes:  %d\n",
		le2_to_uint(tcm_hcd->app_info.num_of_force_elecs));
	if (retval < 0)
		goto exit;

	count += retval;

	retval = count;

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}


#if (USE_KOBJ_SYSFS)
static ssize_t syna_tcm_sysfs_irq_en_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
#else
static ssize_t syna_tcm_sysfs_irq_en_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
#endif
{
	int retval;
	unsigned int input;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm_hcd *tcm_hcd;

	p_kobj = sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm_hcd = dev_get_drvdata(p_dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	mutex_lock(&tcm_hcd->extif_mutex);

	if (input == 0) {
		retval = tcm_hcd->enable_irq(tcm_hcd, false, true);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to disable interrupt\n");
			goto exit;
		}
	} else if (input == 1) {
		retval = tcm_hcd->enable_irq(tcm_hcd, true, NULL);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enable interrupt\n");
			goto exit;
		}
	} else {
		retval = -EINVAL;
		goto exit;
	}

	retval = count;

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

#if (USE_KOBJ_SYSFS)
static ssize_t syna_tcm_sysfs_reset_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
#else
static ssize_t syna_tcm_sysfs_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
#endif
{
	int retval;
	bool hw_reset;
	unsigned int input;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm_hcd *tcm_hcd;

	p_kobj = sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm_hcd = dev_get_drvdata(p_dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input == 1)
		hw_reset = false;
	else if (input == 2)
		hw_reset = true;
	else
		return -EINVAL;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = tcm_hcd->reset_n_reinit(tcm_hcd, hw_reset, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do reset and reinit\n");
		goto exit;
	}

	retval = count;

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static int syna_tcm_set_cur_value(int mode, int val);
static void syna_tcm_esd_recovery(struct syna_tcm_hcd *tcm_hcd);

#if (USE_KOBJ_SYSFS)
static ssize_t syna_tcm_sysfs_cb_debug_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
#else
static ssize_t syna_tcm_sysfs_cb_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
#endif
{
	int retval;
	unsigned int input;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm_hcd *tcm_hcd;
	unsigned int mode, val;

	p_kobj = sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm_hcd = dev_get_drvdata(p_dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	mutex_lock(&tcm_hcd->extif_mutex);

	LOGN(tcm_hcd->pdev->dev.parent,
			"debug get data 0x%02x\n",input);

	mode = input & 0xFF;
	val = (input >> 8) & 0xFF;
	syna_tcm_set_cur_value(mode, val);

	retval = count;

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t syna_tcm_sysfs_test_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int value = 0;

	LOGI(gloab_tcm_hcd->pdev->dev.parent,
		"%s,buf: %s,count: %zu\n", __func__, buf, count);
	sscanf(buf, "%u", &value);
	touch_fod_test(value);
	return count;
}
static DEVICE_ATTR(fod_test, (S_IRUGO | S_IWUSR | S_IWGRP), NULL,\
syna_tcm_sysfs_test_store);


#if (USE_KOBJ_SYSFS)
static ssize_t syna_tcm_sysfs_misc_debug_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
#else
static ssize_t syna_tcm_sysfs_misc_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
#endif
{
	int retval;
	unsigned int input;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm_hcd *tcm_hcd;

	p_kobj = sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm_hcd = dev_get_drvdata(p_dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	mutex_lock(&tcm_hcd->extif_mutex);

	LOGN(tcm_hcd->pdev->dev.parent,
			"misc debug get data 0x%02x\n", input);

	/* 1 -- esd recovery */
	if (input == 1)
		syna_tcm_esd_recovery(tcm_hcd);

	retval = count;

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

#ifdef WATCHDOG_SW
#if (USE_KOBJ_SYSFS)
static ssize_t syna_tcm_sysfs_watchdog_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
#else
static ssize_t syna_tcm_sysfs_watchdog_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
#endif
{
	unsigned int input;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm_hcd *tcm_hcd;

	p_kobj = sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm_hcd = dev_get_drvdata(p_dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 0 && input != 1)
		return -EINVAL;

	mutex_lock(&tcm_hcd->extif_mutex);

	tcm_hcd->watchdog.run = input;
	tcm_hcd->update_watchdog(tcm_hcd, input);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return count;
}
#endif

dynamic_config_sysfs(no_doze, DC_NO_DOZE)

dynamic_config_sysfs(disable_noise_mitigation, DC_DISABLE_NOISE_MITIGATION)

dynamic_config_sysfs(inhibit_frequency_shift, DC_INHIBIT_FREQUENCY_SHIFT)

dynamic_config_sysfs(requested_frequency, DC_REQUESTED_FREQUENCY)

dynamic_config_sysfs(disable_hsync, DC_DISABLE_HSYNC)

dynamic_config_sysfs(rezero_on_exit_deep_sleep, DC_REZERO_ON_EXIT_DEEP_SLEEP)

dynamic_config_sysfs(charger_connected, DC_CHARGER_CONNECTED)

dynamic_config_sysfs(no_baseline_relaxation, DC_NO_BASELINE_RELAXATION)

dynamic_config_sysfs(in_wakeup_gesture_mode, DC_IN_WAKEUP_GESTURE_MODE)

dynamic_config_sysfs(stimulus_fingers, DC_STIMULUS_FINGERS)

dynamic_config_sysfs(grip_suppression_enabled, DC_GRIP_SUPPRESSION_ENABLED)

dynamic_config_sysfs(enable_thick_glove, DC_ENABLE_THICK_GLOVE)

dynamic_config_sysfs(enable_glove, DC_ENABLE_GLOVE)

dynamic_config_sysfs(enable_touch_and_hold, DC_ENABLE_TOUCH_AND_HOLD);

dynamic_config_sysfs(game_mode_ctrl, DC_GAME_MODE_CTRL);

dynamic_config_sysfs(set_report_rate, DC_SET_REPORT_RATE);

dynamic_config_sysfs(enable_gesture_type, DC_GESTURE_TYPE_ENABLE);

int syna_tcm_add_module(struct syna_tcm_module_cb *mod_cb, bool insert)
{
	struct syna_tcm_module_handler *mod_handler;

	if (!mod_pool.initialized) {
		mutex_init(&mod_pool.mutex);
		INIT_LIST_HEAD(&mod_pool.list);
		mod_pool.initialized = true;
	}

	mutex_lock(&mod_pool.mutex);

	if (insert) {
		mod_handler = kzalloc(sizeof(*mod_handler), GFP_KERNEL);
		if (!mod_handler) {
			pr_err("%s: Failed to allocate memory for mod_handler\n",
					__func__);
			mutex_unlock(&mod_pool.mutex);
			return -ENOMEM;
		}
		mod_handler->mod_cb = mod_cb;
		mod_handler->insert = true;
		mod_handler->detach = false;
		list_add_tail(&mod_handler->link, &mod_pool.list);
	} else if (!list_empty(&mod_pool.list)) {
		list_for_each_entry(mod_handler, &mod_pool.list, link) {
			if (mod_handler->mod_cb->type == mod_cb->type) {
				mod_handler->insert = false;
				mod_handler->detach = true;
				goto exit;
			}
		}
	}

exit:
	mutex_unlock(&mod_pool.mutex);

	if (mod_pool.queue_work)
		queue_work(mod_pool.workqueue, &mod_pool.work);

	return 0;
}
EXPORT_SYMBOL(syna_tcm_add_module);

static void syna_tcm_module_work(struct work_struct *work)
{
	struct syna_tcm_module_handler *mod_handler;
	struct syna_tcm_module_handler *tmp_handler;
	struct syna_tcm_hcd *tcm_hcd = mod_pool.tcm_hcd;

	mutex_lock(&mod_pool.mutex);

	if (!list_empty(&mod_pool.list)) {
		list_for_each_entry_safe(mod_handler,
				tmp_handler,
				&mod_pool.list,
				link) {
			if (mod_handler->insert) {
				if (mod_handler->mod_cb->init)
					mod_handler->mod_cb->init(tcm_hcd);
				mod_handler->insert = false;
			}
			if (mod_handler->detach) {
				if (mod_handler->mod_cb->remove)
					mod_handler->mod_cb->remove(tcm_hcd);
				list_del(&mod_handler->link);
				kfree(mod_handler);
			}
		}
	}

	mutex_unlock(&mod_pool.mutex);

	return;
}


#ifdef REPORT_NOTIFIER
/**
 * syna_tcm_report_notifier() - notify occurrence of report received from device
 *
 * @data: handle of core module
 *
 * The occurrence of the report generated by the device is forwarded to the
 * asynchronous inbox of each registered application module.
 */
static int syna_tcm_report_notifier(void *data)
{
	struct sched_param param = { .sched_priority = NOTIFIER_PRIORITY };
	struct syna_tcm_module_handler *mod_handler;
	struct syna_tcm_hcd *tcm_hcd = data;

	sched_setscheduler(current, SCHED_RR, &param);

	set_current_state(TASK_INTERRUPTIBLE);

	while (!kthread_should_stop()) {
		schedule();

		if (kthread_should_stop())
			break;

		set_current_state(TASK_RUNNING);

		mutex_lock(&mod_pool.mutex);

		if (!list_empty(&mod_pool.list)) {
			list_for_each_entry(mod_handler, &mod_pool.list, link) {
				if (!mod_handler->insert &&
						!mod_handler->detach &&
						(mod_handler->mod_cb->asyncbox))
					mod_handler->mod_cb->asyncbox(tcm_hcd);
			}
		}

		mutex_unlock(&mod_pool.mutex);

		set_current_state(TASK_INTERRUPTIBLE);
	};

	return 0;
}
#endif

/**
 * syna_tcm_dispatch_report() - dispatch report received from device
 *
 * @tcm_hcd: handle of core module
 *
 * The report generated by the device is forwarded to the synchronous inbox of
 * each registered application module for further processing. In addition, the
 * report notifier thread is woken up for asynchronous notification of the
 * report occurrence.
 */
static void syna_tcm_dispatch_report(struct syna_tcm_hcd *tcm_hcd)
{
	struct syna_tcm_module_handler *mod_handler;

	LOCK_BUFFER(tcm_hcd->in);
	LOCK_BUFFER(tcm_hcd->report.buffer);

	tcm_hcd->report.buffer.buf = &tcm_hcd->in.buf[MESSAGE_HEADER_SIZE];

	tcm_hcd->report.buffer.buf_size = tcm_hcd->in.buf_size;
	tcm_hcd->report.buffer.buf_size -= MESSAGE_HEADER_SIZE;

	tcm_hcd->report.buffer.data_length = tcm_hcd->payload_length;

	tcm_hcd->report.id = tcm_hcd->status_report_code;

	/* report directly if touch report is received */
	if (tcm_hcd->report.id == REPORT_TOUCH) {
		if (tcm_hcd->report_touch)
			tcm_hcd->report_touch();

	} else {

		/* once an identify report is received, */
		/* reinitialize touch in case any changes */
		if ((tcm_hcd->report.id == REPORT_IDENTIFY) &&
				IS_FW_MODE(tcm_hcd->id_info.mode)) {

			if (atomic_read(&tcm_hcd->helper.task) == HELP_NONE) {
				atomic_set(&tcm_hcd->helper.task,
						HELP_TOUCH_REINIT);
				queue_work(tcm_hcd->helper.workqueue,
						&tcm_hcd->helper.work);
			}
		}

		/* dispatch received report to the other modules */
		mutex_lock(&mod_pool.mutex);

		if (!list_empty(&mod_pool.list)) {
			list_for_each_entry(mod_handler, &mod_pool.list, link) {
				if (!mod_handler->insert &&
						!mod_handler->detach &&
						(mod_handler->mod_cb->syncbox))
					mod_handler->mod_cb->syncbox(tcm_hcd);
			}
		}

		tcm_hcd->async_report_id = tcm_hcd->status_report_code;

		mutex_unlock(&mod_pool.mutex);
	}

	UNLOCK_BUFFER(tcm_hcd->report.buffer);
	UNLOCK_BUFFER(tcm_hcd->in);

#ifdef REPORT_NOTIFIER
	wake_up_process(tcm_hcd->notifier_thread);
#endif

	return;
}

/**
 * syna_tcm_dispatch_response() - dispatch response received from device
 *
 * @tcm_hcd: handle of core module
 *
 * The response to a command is forwarded to the sender of the command.
 */
static void syna_tcm_dispatch_response(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;

	if (atomic_read(&tcm_hcd->command_status) != CMD_BUSY)
		return;

	tcm_hcd->response_code = tcm_hcd->status_report_code;

	if (tcm_hcd->payload_length == 0) {
		atomic_set(&tcm_hcd->command_status, CMD_IDLE);
		goto exit;
	}

	LOCK_BUFFER(tcm_hcd->resp);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&tcm_hcd->resp,
			tcm_hcd->payload_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for tcm_hcd->resp.buf\n");
		UNLOCK_BUFFER(tcm_hcd->resp);
		atomic_set(&tcm_hcd->command_status, CMD_ERROR);
		goto exit;
	}

	LOCK_BUFFER(tcm_hcd->in);

	retval = secure_memcpy(tcm_hcd->resp.buf,
			tcm_hcd->resp.buf_size,
			&tcm_hcd->in.buf[MESSAGE_HEADER_SIZE],
			tcm_hcd->in.buf_size - MESSAGE_HEADER_SIZE,
			tcm_hcd->payload_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy payload\n");
		UNLOCK_BUFFER(tcm_hcd->in);
		UNLOCK_BUFFER(tcm_hcd->resp);
		atomic_set(&tcm_hcd->command_status, CMD_ERROR);
		goto exit;
	}

	tcm_hcd->resp.data_length = tcm_hcd->payload_length;

	UNLOCK_BUFFER(tcm_hcd->in);
	UNLOCK_BUFFER(tcm_hcd->resp);

	atomic_set(&tcm_hcd->command_status, CMD_IDLE);

exit:
	complete(&response_complete);

	return;
}

/**
 * syna_tcm_dispatch_message() - dispatch message received from device
 *
 * @tcm_hcd: handle of core module
 *
 * The information received in the message read in from the device is dispatched
 * to the appropriate destination based on whether the information represents a
 * report or a response to a command.
 */
static void syna_tcm_dispatch_message(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *build_id;
	unsigned int payload_length;
	unsigned int max_write_size;

	if (tcm_hcd->status_report_code == REPORT_IDENTIFY) {
		payload_length = tcm_hcd->payload_length;

		LOCK_BUFFER(tcm_hcd->in);

		retval = secure_memcpy((unsigned char *)&tcm_hcd->id_info,
				sizeof(tcm_hcd->id_info),
				&tcm_hcd->in.buf[MESSAGE_HEADER_SIZE],
				tcm_hcd->in.buf_size - MESSAGE_HEADER_SIZE,
				MIN(sizeof(tcm_hcd->id_info), payload_length));
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy identification info\n");
			UNLOCK_BUFFER(tcm_hcd->in);
			return;
		}

		UNLOCK_BUFFER(tcm_hcd->in);

		build_id = tcm_hcd->id_info.build_id;
		tcm_hcd->packrat_number = le4_to_uint(build_id);

		max_write_size = le2_to_uint(tcm_hcd->id_info.max_write_size);
		tcm_hcd->wr_chunk_size = MIN(max_write_size, WR_CHUNK_SIZE);
		if (tcm_hcd->wr_chunk_size == 0)
			tcm_hcd->wr_chunk_size = max_write_size;

		LOGN(tcm_hcd->pdev->dev.parent,
				"Received identify report (firmware mode = 0x%02x)\n",
				tcm_hcd->id_info.mode);

		if (atomic_read(&tcm_hcd->command_status) == CMD_BUSY) {
			switch (tcm_hcd->command) {
			case CMD_RESET:
			case CMD_RUN_BOOTLOADER_FIRMWARE:
			case CMD_RUN_APPLICATION_FIRMWARE:
			case CMD_ENTER_PRODUCTION_TEST_MODE:
			case CMD_ROMBOOT_RUN_BOOTLOADER_FIRMWARE:
				tcm_hcd->response_code = STATUS_OK;
				atomic_set(&tcm_hcd->command_status, CMD_IDLE);
				complete(&response_complete);
				break;
			default:
				LOGN(tcm_hcd->pdev->dev.parent,
						"Device has been reset\n");
				atomic_set(&tcm_hcd->command_status, CMD_ERROR);
				complete(&response_complete);
				break;
			}
		} else {

			if ((tcm_hcd->id_info.mode == MODE_ROMBOOTLOADER) &&
					tcm_hcd->in_hdl_mode) {

				if (atomic_read(&tcm_hcd->helper.task) ==
						HELP_NONE) {
					atomic_set(&tcm_hcd->helper.task,
							HELP_SEND_ROMBOOT_HDL);
					queue_work(tcm_hcd->helper.workqueue,
							&tcm_hcd->helper.work);
				} else {
					LOGN(tcm_hcd->pdev->dev.parent,
							"Helper thread is busy\n");
				}
				return;
			}
		}

#ifdef FORCE_RUN_APPLICATION_FIRMWARE
		if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode) &&
				!mutex_is_locked(&tcm_hcd->reset_mutex)) {

			if (atomic_read(&tcm_hcd->helper.task) == HELP_NONE) {
				atomic_set(&tcm_hcd->helper.task,
						HELP_RUN_APPLICATION_FIRMWARE);
				queue_work(tcm_hcd->helper.workqueue,
						&tcm_hcd->helper.work);
				return;
			}
		}
#endif

		/* To avoid the identify report dispatching during the HDL. */
		if (atomic_read(&tcm_hcd->host_downloading)) {
			LOGN(tcm_hcd->pdev->dev.parent,
					"Switched to TCM mode and going to download the configs\n");
			return;
		}
	}


	if (tcm_hcd->status_report_code >= REPORT_IDENTIFY)
		syna_tcm_dispatch_report(tcm_hcd);
	else
		syna_tcm_dispatch_response(tcm_hcd);

	return;
}

/**
 * syna_tcm_continued_read() - retrieve entire payload from device
 *
 * @tcm_hcd: handle of core module
 *
 * Read transactions are carried out until the entire payload is retrieved from
 * the device and stored in the handle of the core module.
 */
static int syna_tcm_continued_read(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char marker;
	unsigned char code;
	unsigned int idx;
	unsigned int offset;
	unsigned int chunks;
	unsigned int chunk_space;
	unsigned int xfer_length;
	unsigned int total_length;
	unsigned int remaining_length;

	total_length = MESSAGE_HEADER_SIZE + tcm_hcd->payload_length + 1;

	remaining_length = total_length - tcm_hcd->read_length;

	LOCK_BUFFER(tcm_hcd->in);

	retval = syna_tcm_realloc_mem(tcm_hcd,
			&tcm_hcd->in,
			total_length + 1);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to reallocate memory for tcm_hcd->in.buf\n");
		UNLOCK_BUFFER(tcm_hcd->in);
		return retval;
	}

	/* available chunk space for payload = total chunk size minus header
	 * marker byte and header code byte */
	if (tcm_hcd->rd_chunk_size == 0)
		chunk_space = remaining_length;
	else
		chunk_space = tcm_hcd->rd_chunk_size - 2;

	chunks = ceil_div(remaining_length, chunk_space);

	chunks = chunks == 0 ? 1 : chunks;

	offset = tcm_hcd->read_length;

	LOCK_BUFFER(tcm_hcd->temp);

	for (idx = 0; idx < chunks; idx++) {
		if (remaining_length > chunk_space)
			xfer_length = chunk_space;
		else
			xfer_length = remaining_length;

		if (xfer_length == 1) {
			tcm_hcd->in.buf[offset] = MESSAGE_PADDING;
			offset += xfer_length;
			remaining_length -= xfer_length;
			continue;
		}

		retval = syna_tcm_alloc_mem(tcm_hcd,
				&tcm_hcd->temp,
				xfer_length + 2);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for tcm_hcd->temp.buf\n");
			UNLOCK_BUFFER(tcm_hcd->temp);
			UNLOCK_BUFFER(tcm_hcd->in);
			return retval;
		}

		retval = syna_tcm_read(tcm_hcd,
				tcm_hcd->temp.buf,
				xfer_length + 2);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to read from device\n");
			UNLOCK_BUFFER(tcm_hcd->temp);
			UNLOCK_BUFFER(tcm_hcd->in);
			return retval;
		}

		marker = tcm_hcd->temp.buf[0];
		code = tcm_hcd->temp.buf[1];

		if (marker != MESSAGE_MARKER) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Incorrect header marker (0x%02x)\n",
					marker);
			UNLOCK_BUFFER(tcm_hcd->temp);
			UNLOCK_BUFFER(tcm_hcd->in);
			return -EIO;
		}

		if (code != STATUS_CONTINUED_READ) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Incorrect header code (0x%02x)\n",
					code);
			UNLOCK_BUFFER(tcm_hcd->temp);
			UNLOCK_BUFFER(tcm_hcd->in);
			return -EIO;
		}

		retval = secure_memcpy(&tcm_hcd->in.buf[offset],
				tcm_hcd->in.buf_size - offset,
				&tcm_hcd->temp.buf[2],
				tcm_hcd->temp.buf_size - 2,
				xfer_length);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy payload\n");
			UNLOCK_BUFFER(tcm_hcd->temp);
			UNLOCK_BUFFER(tcm_hcd->in);
			return retval;
		}

		offset += xfer_length;

		remaining_length -= xfer_length;
	}

	UNLOCK_BUFFER(tcm_hcd->temp);
	UNLOCK_BUFFER(tcm_hcd->in);

	return 0;
}

/**
 * syna_tcm_raw_read() - retrieve specific number of data bytes from device
 *
 * @tcm_hcd: handle of core module
 * @in_buf: buffer for storing data retrieved from device
 * @length: number of bytes to retrieve from device
 *
 * Read transactions are carried out until the specific number of data bytes are
 * retrieved from the device and stored in in_buf.
 */
static int syna_tcm_raw_read(struct syna_tcm_hcd *tcm_hcd,
		unsigned char *in_buf, unsigned int length)
{
	int retval;
	unsigned char code;
	unsigned int idx;
	unsigned int offset;
	unsigned int chunks;
	unsigned int chunk_space;
	unsigned int xfer_length;
	unsigned int remaining_length;

	LOGN(tcm_hcd->pdev->dev.parent, "-----enter-----length:%d\n", length);

	if (length < 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid length information\n");
		return -EINVAL;
	}

	/* minus header marker byte and header code byte */
	remaining_length = length - 2;

	/* available chunk space for data = total chunk size minus header marker
	 * byte and header code byte */
	if (tcm_hcd->rd_chunk_size == 0)
		chunk_space = remaining_length;
	else
		chunk_space = tcm_hcd->rd_chunk_size - 2;

	chunks = ceil_div(remaining_length, chunk_space);

	chunks = chunks == 0 ? 1 : chunks;

	offset = 0;

	LOCK_BUFFER(tcm_hcd->temp);

	for (idx = 0; idx < chunks; idx++) {
		if (remaining_length > chunk_space)
			xfer_length = chunk_space;
		else
			xfer_length = remaining_length;

		if (xfer_length == 1) {
			in_buf[offset] = MESSAGE_PADDING;
			offset += xfer_length;
			remaining_length -= xfer_length;
			continue;
		}

		retval = syna_tcm_alloc_mem(tcm_hcd,
				&tcm_hcd->temp,
				xfer_length + 2);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for tcm_hcd->temp.buf\n");
			UNLOCK_BUFFER(tcm_hcd->temp);
			return retval;
		}

		retval = syna_tcm_read(tcm_hcd,
				tcm_hcd->temp.buf,
				xfer_length + 2);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to read from device\n");
			UNLOCK_BUFFER(tcm_hcd->temp);
			return retval;
		}

		code = tcm_hcd->temp.buf[1];

		if (idx == 0) {
			retval = secure_memcpy(&in_buf[0],
					length,
					&tcm_hcd->temp.buf[0],
					tcm_hcd->temp.buf_size,
					xfer_length + 2);
		} else {
			if (code != STATUS_CONTINUED_READ) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Incorrect header code (0x%02x)\n",
						code);
				UNLOCK_BUFFER(tcm_hcd->temp);
				return -EIO;
			}

			retval = secure_memcpy(&in_buf[offset],
					length - offset,
					&tcm_hcd->temp.buf[2],
					tcm_hcd->temp.buf_size - 2,
					xfer_length);
		}
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy data\n");
			UNLOCK_BUFFER(tcm_hcd->temp);
			return retval;
		}

		if (idx == 0)
			offset += (xfer_length + 2);
		else
			offset += xfer_length;

		remaining_length -= xfer_length;
	}

	UNLOCK_BUFFER(tcm_hcd->temp);

	return 0;
}

/**
 * syna_tcm_raw_write() - write command/data to device without receiving
 * response
 *
 * @tcm_hcd: handle of core module
 * @command: command to send to device
 * @data: data to send to device
 * @length: length of data in bytes
 *
 * A command and its data, if any, are sent to the device.
 */
static int syna_tcm_raw_write(struct syna_tcm_hcd *tcm_hcd,
		unsigned char command, unsigned char *data, unsigned int length)
{
	int retval;
	unsigned int idx;
	unsigned int chunks;
	unsigned int chunk_space;
	unsigned int xfer_length;
	unsigned int remaining_length;

	remaining_length = length;

	/* available chunk space for data = total chunk size minus command
	 * byte */
	if (tcm_hcd->wr_chunk_size == 0)
		chunk_space = remaining_length;
	else
		chunk_space = tcm_hcd->wr_chunk_size - 1;

	chunks = ceil_div(remaining_length, chunk_space);

	chunks = chunks == 0 ? 1 : chunks;

	LOCK_BUFFER(tcm_hcd->out);

	for (idx = 0; idx < chunks; idx++) {
		if (remaining_length > chunk_space)
			xfer_length = chunk_space;
		else
			xfer_length = remaining_length;

		retval = syna_tcm_alloc_mem(tcm_hcd,
				&tcm_hcd->out,
				xfer_length + 1);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for tcm_hcd->out.buf\n");
			UNLOCK_BUFFER(tcm_hcd->out);
			return retval;
		}

		if (idx == 0)
			tcm_hcd->out.buf[0] = command;
		else
			tcm_hcd->out.buf[0] = CMD_CONTINUE_WRITE;

		if (xfer_length) {
			retval = secure_memcpy(&tcm_hcd->out.buf[1],
					tcm_hcd->out.buf_size - 1,
					&data[idx * chunk_space],
					remaining_length,
					xfer_length);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to copy data\n");
				UNLOCK_BUFFER(tcm_hcd->out);
				return retval;
			}
		}

		retval = syna_tcm_write(tcm_hcd,
				tcm_hcd->out.buf,
				xfer_length + 1);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to write to device\n");
			UNLOCK_BUFFER(tcm_hcd->out);
			return retval;
		}

		remaining_length -= xfer_length;
	}

	UNLOCK_BUFFER(tcm_hcd->out);

	return 0;
}

/**
 * syna_tcm_read_message() - read message from device
 *
 * @tcm_hcd: handle of core module
 * @in_buf: buffer for storing data in raw read mode
 * @length: length of data in bytes in raw read mode
 *
 * If in_buf is not NULL, raw read mode is used and syna_tcm_raw_read() is
 * called. Otherwise, a message including its entire payload is retrieved from
 * the device and dispatched to the appropriate destination.
 */
static int syna_tcm_read_message(struct syna_tcm_hcd *tcm_hcd,
		unsigned char *in_buf, unsigned int length)
{
	int retval;
	bool retry;
	unsigned int total_length;
	struct syna_tcm_message_header *header;
	int retry_cnt = 0;

	mutex_lock(&tcm_hcd->rw_ctrl_mutex);

	if (in_buf != NULL) {
		retval = syna_tcm_raw_read(tcm_hcd, in_buf, length);
		goto exit;
	}

	retry = true;

retry:
	LOCK_BUFFER(tcm_hcd->in);

	retval = syna_tcm_read(tcm_hcd,
			tcm_hcd->in.buf,
			tcm_hcd->read_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read from device\n");
		UNLOCK_BUFFER(tcm_hcd->in);
		if (retry) {
			usleep_range(READ_RETRY_US_MIN, READ_RETRY_US_MAX);
			retry = false;
			goto retry;
		}
		goto exit;
	}

	header = (struct syna_tcm_message_header *)tcm_hcd->in.buf;


	if (header->marker != MESSAGE_MARKER) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Incorrect header marker (0x%02x)\n",
				header->marker);
		UNLOCK_BUFFER(tcm_hcd->in);
		retval = -ENXIO;
		if (retry) {
			usleep_range(READ_RETRY_US_MIN, READ_RETRY_US_MAX);
			//retry = false;
			if(retry_cnt < 20)
			{
				retry_cnt++;
			}else{
				retry_cnt = 0;
				retry = false;
			}
			goto retry;
		}
		goto exit;
	}

	tcm_hcd->status_report_code = header->code;

	tcm_hcd->payload_length = le2_to_uint(header->length);

	LOGD(tcm_hcd->pdev->dev.parent,
			"Status report code = 0x%02x\n",
			tcm_hcd->status_report_code);

	LOGD(tcm_hcd->pdev->dev.parent,
			"Payload length = %d\n",
			tcm_hcd->payload_length);

	if (tcm_hcd->status_report_code <= STATUS_ERROR ||
			tcm_hcd->status_report_code == STATUS_INVALID) {
		switch (tcm_hcd->status_report_code) {
		case STATUS_OK:
			break;
		case STATUS_CONTINUED_READ:
			LOGD(tcm_hcd->pdev->dev.parent,
					"Out-of-sync continued read\n");
		case STATUS_IDLE:
		case STATUS_BUSY:
			tcm_hcd->payload_length = 0;
			UNLOCK_BUFFER(tcm_hcd->in);
			retval = 0;
			goto exit;
		default:
			LOGE(tcm_hcd->pdev->dev.parent,
					"Incorrect Status code (0x%02x)\n",
					tcm_hcd->status_report_code);
			if (tcm_hcd->status_report_code == STATUS_INVALID) {
				if (retry) {
					usleep_range(READ_RETRY_US_MIN,
							READ_RETRY_US_MAX);
					retry = false;
					goto retry;
				} else {
					tcm_hcd->payload_length = 0;
				}
			}
		}
	}

	total_length = MESSAGE_HEADER_SIZE + tcm_hcd->payload_length + 1;

#ifdef PREDICTIVE_READING
	if (total_length <= tcm_hcd->read_length) {
		goto check_padding;
	} else if (total_length - 1 == tcm_hcd->read_length) {
		tcm_hcd->in.buf[total_length - 1] = MESSAGE_PADDING;
		goto check_padding;
	}
#else
	if (tcm_hcd->payload_length == 0) {
		tcm_hcd->in.buf[total_length - 1] = MESSAGE_PADDING;
		goto check_padding;
	}
#endif

	UNLOCK_BUFFER(tcm_hcd->in);

	retval = syna_tcm_continued_read(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do continued read\n");
		goto exit;
	};

	LOCK_BUFFER(tcm_hcd->in);

	tcm_hcd->in.buf[0] = MESSAGE_MARKER;
	tcm_hcd->in.buf[1] = tcm_hcd->status_report_code;
	tcm_hcd->in.buf[2] = (unsigned char)tcm_hcd->payload_length;
	tcm_hcd->in.buf[3] = (unsigned char)(tcm_hcd->payload_length >> 8);

check_padding:
	if (tcm_hcd->in.buf[total_length - 1] != MESSAGE_PADDING) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Incorrect message padding byte (0x%02x)\n",
				tcm_hcd->in.buf[total_length - 1]);
		UNLOCK_BUFFER(tcm_hcd->in);
		retval = -EIO;
		goto exit;
	}

	UNLOCK_BUFFER(tcm_hcd->in);

#ifdef PREDICTIVE_READING
	total_length = MAX(total_length, MIN_READ_LENGTH);
	tcm_hcd->read_length = MIN(total_length, tcm_hcd->rd_chunk_size);
	if (tcm_hcd->rd_chunk_size == 0)
		tcm_hcd->read_length = total_length;
#endif
	if (tcm_hcd->is_detected)
		syna_tcm_dispatch_message(tcm_hcd);

	retval = 0;

exit:
	if (retval < 0) {
		if (atomic_read(&tcm_hcd->command_status) == CMD_BUSY) {
			atomic_set(&tcm_hcd->command_status, CMD_ERROR);
			complete(&response_complete);
		}
	}

	mutex_unlock(&tcm_hcd->rw_ctrl_mutex);

	return retval;
}

/**
 * syna_tcm_write_message() - write message to device and receive response
 *
 * @tcm_hcd: handle of core module
 * @command: command to send to device
 * @payload: payload of command
 * @length: length of payload in bytes
 * @resp_buf: buffer for storing command response
 * @resp_buf_size: size of response buffer in bytes
 * @resp_length: length of command response in bytes
 * @response_code: status code returned in command response
 * @polling_delay_ms: delay time after sending command before resuming polling
 *
 * If resp_buf is NULL, raw write mode is used and syna_tcm_raw_write() is
 * called. Otherwise, a command and its payload, if any, are sent to the device
 * and the response to the command generated by the device is read in.
 */
static int syna_tcm_write_message(struct syna_tcm_hcd *tcm_hcd,
		unsigned char command, unsigned char *payload,
		unsigned int length, unsigned char **resp_buf,
		unsigned int *resp_buf_size, unsigned int *resp_length,
		unsigned char *response_code, unsigned int polling_delay_ms)
{
	int retval;
	unsigned int idx;
	unsigned int chunks;
	unsigned int chunk_space;
	unsigned int xfer_length;
	unsigned int remaining_length;
	unsigned int command_status;
	bool is_romboot_hdl = (command == CMD_ROMBOOT_DOWNLOAD) ? true : false;
	bool is_hdl_reset = (command == CMD_RESET) && (tcm_hcd->in_hdl_mode);

	if (response_code != NULL)
		*response_code = STATUS_INVALID;

	if (!tcm_hcd->do_polling && current->pid == tcm_hcd->isr_pid) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid execution context\n");
		return -EINVAL;
	}

	mutex_lock(&tcm_hcd->command_mutex);

	mutex_lock(&tcm_hcd->rw_ctrl_mutex);

	if (resp_buf == NULL) {
		retval = syna_tcm_raw_write(tcm_hcd, command, payload, length);
		mutex_unlock(&tcm_hcd->rw_ctrl_mutex);
		goto exit;
	}

	if (tcm_hcd->do_polling && polling_delay_ms) {
		cancel_delayed_work_sync(&tcm_hcd->polling_work);
		flush_workqueue(tcm_hcd->polling_workqueue);
	}

	atomic_set(&tcm_hcd->command_status, CMD_BUSY);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
	reinit_completion(&response_complete);
#else
	INIT_COMPLETION(response_complete);
#endif

	tcm_hcd->command = command;

	LOCK_BUFFER(tcm_hcd->resp);

	tcm_hcd->resp.buf = *resp_buf;
	tcm_hcd->resp.buf_size = *resp_buf_size;
	tcm_hcd->resp.data_length = 0;

	UNLOCK_BUFFER(tcm_hcd->resp);

	/* adding two length bytes as part of payload */
	remaining_length = length + 2;

	/* available chunk space for payload = total chunk size minus command
	 * byte */
	if (tcm_hcd->wr_chunk_size == 0)
		chunk_space = remaining_length;
	else
		chunk_space = tcm_hcd->wr_chunk_size - 1;

	if (is_romboot_hdl) {
		if (WR_CHUNK_SIZE) {
			chunk_space = WR_CHUNK_SIZE - 1;
			chunk_space = chunk_space -
					(chunk_space % ROMBOOT_DOWNLOAD_UNIT);
		} else {
			chunk_space = remaining_length;
		}
	}

	chunks = ceil_div(remaining_length, chunk_space);

	chunks = chunks == 0 ? 1 : chunks;

	LOGD(tcm_hcd->pdev->dev.parent,
			"Command = 0x%02x\n",
			command);

	LOCK_BUFFER(tcm_hcd->out);

	for (idx = 0; idx < chunks; idx++) {
		if (remaining_length > chunk_space)
			xfer_length = chunk_space;
		else
			xfer_length = remaining_length;

		retval = syna_tcm_alloc_mem(tcm_hcd,
				&tcm_hcd->out,
				xfer_length + 1);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for tcm_hcd->out.buf\n");
			UNLOCK_BUFFER(tcm_hcd->out);
			mutex_unlock(&tcm_hcd->rw_ctrl_mutex);
			goto exit;
		}

		if (idx == 0) {
			tcm_hcd->out.buf[0] = command;
			tcm_hcd->out.buf[1] = (unsigned char)length;
			tcm_hcd->out.buf[2] = (unsigned char)(length >> 8);

			if (xfer_length > 2) {
				retval = secure_memcpy(&tcm_hcd->out.buf[3],
						tcm_hcd->out.buf_size - 3,
						payload,
						remaining_length - 2,
						xfer_length - 2);
				if (retval < 0) {
					LOGE(tcm_hcd->pdev->dev.parent,
							"Failed to copy payload\n");
					UNLOCK_BUFFER(tcm_hcd->out);
					mutex_unlock(&tcm_hcd->rw_ctrl_mutex);
					goto exit;
				}
			}
		} else {
			tcm_hcd->out.buf[0] = CMD_CONTINUE_WRITE;

			retval = secure_memcpy(&tcm_hcd->out.buf[1],
					tcm_hcd->out.buf_size - 1,
					&payload[idx * chunk_space - 2],
					remaining_length,
					xfer_length);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to copy payload\n");
				UNLOCK_BUFFER(tcm_hcd->out);
				mutex_unlock(&tcm_hcd->rw_ctrl_mutex);
				goto exit;
			}
		}

/* 		LOGE(tcm_hcd->pdev->dev.parent,
					"Before cmd = 0x%02x\n", command); */
		retval = syna_tcm_write(tcm_hcd,
				tcm_hcd->out.buf,
				xfer_length + 1);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to write to device\n");
			UNLOCK_BUFFER(tcm_hcd->out);
			mutex_unlock(&tcm_hcd->rw_ctrl_mutex);
			goto exit;
		}
/* 		LOGE(tcm_hcd->pdev->dev.parent,
					"After cmd = 0x%02x\n", command); */
		remaining_length -= xfer_length;

		if (chunks > 1)
			usleep_range(WRITE_DELAY_US_MIN, WRITE_DELAY_US_MAX);
	}

	UNLOCK_BUFFER(tcm_hcd->out);

	mutex_unlock(&tcm_hcd->rw_ctrl_mutex);

	if (is_hdl_reset)
		goto exit;

	if (tcm_hcd->do_polling && polling_delay_ms) {
		queue_delayed_work(tcm_hcd->polling_workqueue,
				&tcm_hcd->polling_work,
				msecs_to_jiffies(polling_delay_ms));
	}

	retval = wait_for_completion_timeout(&response_complete,
			msecs_to_jiffies(RESPONSE_TIMEOUT_MS));
	if (retval == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Timed out waiting for response (command 0x%02x), irq: %s, ATTN: %d\n",
				tcm_hcd->command,
				(tcm_hcd->irq_enabled) ? STR(TRUE) : STR(FALSE),
				gpio_get_value(tcm_hcd->hw_if->bdata->irq_gpio));
		retval = -ETIME;
		goto exit;
	}

	command_status = atomic_read(&tcm_hcd->command_status);
	if (command_status != CMD_IDLE) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get valid response (command 0x%02x)\n",
				tcm_hcd->command);
		retval = -EIO;
		goto exit;
	}

	LOCK_BUFFER(tcm_hcd->resp);

	if (tcm_hcd->response_code != STATUS_OK) {
		if (tcm_hcd->resp.data_length) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Error code = 0x%02x (command 0x%02x)\n",
					tcm_hcd->resp.buf[0], tcm_hcd->command);
		}
		retval = -EIO;
	} else {
		retval = 0;
	}

	*resp_buf = tcm_hcd->resp.buf;
	*resp_buf_size = tcm_hcd->resp.buf_size;
	*resp_length = tcm_hcd->resp.data_length;

	if (response_code != NULL)
		*response_code = tcm_hcd->response_code;

	UNLOCK_BUFFER(tcm_hcd->resp);

exit:
	tcm_hcd->command = CMD_NONE;

	atomic_set(&tcm_hcd->command_status, CMD_IDLE);

	mutex_unlock(&tcm_hcd->command_mutex);

	return retval;
}

static int syna_tcm_wait_hdl(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;

	msleep(HOST_DOWNLOAD_WAIT_MS);

	if (!atomic_read(&tcm_hcd->host_downloading))
		return 0;

	retval = wait_event_interruptible_timeout(tcm_hcd->hdl_wq,
			!atomic_read(&tcm_hcd->host_downloading),
			msecs_to_jiffies(HOST_DOWNLOAD_TIMEOUT_MS));
	if (retval == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Timed out waiting for completion of host download\n");
		atomic_set(&tcm_hcd->host_downloading, 0);
		retval = -EIO;
	} else {
		retval = 0;
	}

	return retval;
}

static void syna_tcm_check_hdl(struct syna_tcm_hcd *tcm_hcd, unsigned char id)
{
	struct syna_tcm_module_handler *mod_handler;

	LOCK_BUFFER(tcm_hcd->report.buffer);

	tcm_hcd->report.buffer.buf = NULL;
	tcm_hcd->report.buffer.buf_size = 0;
	tcm_hcd->report.buffer.data_length = 0;
	tcm_hcd->report.id = id;

	UNLOCK_BUFFER(tcm_hcd->report.buffer);

	mutex_lock(&mod_pool.mutex);

	if (!list_empty(&mod_pool.list)) {
		list_for_each_entry(mod_handler, &mod_pool.list, link) {
			if (!mod_handler->insert &&
					!mod_handler->detach &&
					(mod_handler->mod_cb->syncbox))
				mod_handler->mod_cb->syncbox(tcm_hcd);
		}
	}

	mutex_unlock(&mod_pool.mutex);

	return;
}

#ifdef WATCHDOG_SW
static void syna_tcm_update_watchdog(struct syna_tcm_hcd *tcm_hcd, bool en)
{
	cancel_delayed_work_sync(&tcm_hcd->watchdog.work);
	flush_workqueue(tcm_hcd->watchdog.workqueue);

	if (!tcm_hcd->watchdog.run) {
		tcm_hcd->watchdog.count = 0;
		return;
	}

	if (en) {
		queue_delayed_work(tcm_hcd->watchdog.workqueue,
				&tcm_hcd->watchdog.work,
				msecs_to_jiffies(WATCHDOG_DELAY_MS));
	} else {
		tcm_hcd->watchdog.count = 0;
	}

	return;
}

static void syna_tcm_watchdog_work(struct work_struct *work)
{
	int retval;
	unsigned char marker;
	struct delayed_work *delayed_work =
			container_of(work, struct delayed_work, work);
	struct syna_tcm_watchdog *watchdog =
			container_of(delayed_work, struct syna_tcm_watchdog,
			work);
	struct syna_tcm_hcd *tcm_hcd =
			container_of(watchdog, struct syna_tcm_hcd, watchdog);

	if (mutex_is_locked(&tcm_hcd->rw_ctrl_mutex))
		goto exit;

	mutex_lock(&tcm_hcd->rw_ctrl_mutex);

	retval = syna_tcm_read(tcm_hcd,
			&marker,
			1);

	mutex_unlock(&tcm_hcd->rw_ctrl_mutex);

	if (retval < 0 || marker != MESSAGE_MARKER) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read from device\n");

		tcm_hcd->watchdog.count++;

		if (tcm_hcd->watchdog.count >= WATCHDOG_TRIGGER_COUNT) {
			retval = tcm_hcd->reset_n_reinit(tcm_hcd, true, false);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to do reset and reinit\n");
			}
			tcm_hcd->watchdog.count = 0;
		}
	}

exit:
	queue_delayed_work(tcm_hcd->watchdog.workqueue,
			&tcm_hcd->watchdog.work,
			msecs_to_jiffies(WATCHDOG_DELAY_MS));

	return;
}
#endif

static void syna_tcm_polling_work(struct work_struct *work)
{
	int retval;
	struct delayed_work *delayed_work =
			container_of(work, struct delayed_work, work);
	struct syna_tcm_hcd *tcm_hcd =
			container_of(delayed_work, struct syna_tcm_hcd,
			polling_work);

	if (!tcm_hcd->do_polling)
		return;

	retval = tcm_hcd->read_message(tcm_hcd,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read message\n");
		if (retval == -ENXIO && tcm_hcd->hw_if->bus_io->type == BUS_SPI)
			syna_tcm_check_hdl(tcm_hcd, REPORT_HDL_F35);
	}

	if (!(tcm_hcd->in_suspend && retval < 0)) {
		queue_delayed_work(tcm_hcd->polling_workqueue,
				&tcm_hcd->polling_work,
				msecs_to_jiffies(POLLING_DELAY_MS));
	}

	return;
}

static irqreturn_t syna_tcm_isr(int irq, void *data)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = data;
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	if (unlikely(gpio_get_value(bdata->irq_gpio) != bdata->irq_on_state))
		goto exit;

	tcm_hcd->isr_pid = current->pid;

	retval = tcm_hcd->read_message(tcm_hcd,
			NULL,
			0);

	if (retval < 0) {
		if (tcm_hcd->sensor_type == TYPE_F35)
			syna_tcm_check_hdl(tcm_hcd, REPORT_HDL_F35);
		else
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read message\n");
	}

exit:
	return IRQ_HANDLED;
}

static int syna_tcm_enable_irq(struct syna_tcm_hcd *tcm_hcd, bool en, bool ns)
{
	int retval;
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;
	static bool irq_freed = true;

	mutex_lock(&tcm_hcd->irq_en_mutex);

	if (en) {
		if (tcm_hcd->irq_enabled) {
			LOGD(tcm_hcd->pdev->dev.parent,
					"Interrupt already enabled\n");
			retval = 0;
			goto exit;
		}

		if (bdata->irq_gpio < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Invalid IRQ GPIO\n");
			retval = -EINVAL;
			goto queue_polling_work;
		}

		if (irq_freed) {
			retval = request_threaded_irq(tcm_hcd->irq, NULL,
					syna_tcm_isr, bdata->irq_flags,
					PLATFORM_DRIVER_NAME, tcm_hcd);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to create interrupt thread\n");
			}
		} else {
			enable_irq(tcm_hcd->irq);
			retval = 0;
		}

queue_polling_work:
		if (retval < 0) {
#ifdef FALL_BACK_ON_POLLING
			queue_delayed_work(tcm_hcd->polling_workqueue,
					&tcm_hcd->polling_work,
					msecs_to_jiffies(POLLING_DELAY_MS));
			tcm_hcd->do_polling = true;
			retval = 0;
#endif
		}

		if (retval < 0)
			goto exit;
		else
			msleep(ENABLE_IRQ_DELAY_MS);
	} else {
		if (!tcm_hcd->irq_enabled) {
			LOGD(tcm_hcd->pdev->dev.parent,
					"Interrupt already disabled\n");
			retval = 0;
			goto exit;
		}

		if (bdata->irq_gpio >= 0) {
			if (ns) {
				disable_irq_nosync(tcm_hcd->irq);
			} else {
				disable_irq(tcm_hcd->irq);
				free_irq(tcm_hcd->irq, tcm_hcd);
			}
			irq_freed = !ns;
		}

		if (ns) {
			cancel_delayed_work(&tcm_hcd->polling_work);
		} else {
			cancel_delayed_work_sync(&tcm_hcd->polling_work);
			flush_workqueue(tcm_hcd->polling_workqueue);
		}

		tcm_hcd->do_polling = false;
	}

	retval = 0;

exit:
	if (retval == 0)
		tcm_hcd->irq_enabled = en;

	mutex_unlock(&tcm_hcd->irq_en_mutex);

	return retval;
}

static int syna_tcm_set_gpio(struct syna_tcm_hcd *tcm_hcd, int gpio,
		bool config, int dir, int state)
{
	int retval;
	char label[16];

	if (config) {
		retval = snprintf(label, 16, "tcm_gpio_%d\n", gpio);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to set GPIO label\n");
			return retval;
		}

		LOGN(tcm_hcd->pdev->dev.parent, "label----%s:\n",label);

		retval = gpio_request(gpio, label);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to request GPIO %d\n",
					gpio);
			return retval;
		}

		if (dir == 0)
			retval = gpio_direction_input(gpio);
		else
			retval = gpio_direction_output(gpio, state);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to set GPIO %d direction\n",
					gpio);
			return retval;
		}
	} else {
		gpio_free(gpio);
	}

	return 0;
}

static int syna_tcm_config_gpio(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	if (bdata->irq_gpio >= 0) {
		retval = syna_tcm_set_gpio(tcm_hcd, bdata->irq_gpio,
				true, 0, 0);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to configure interrupt GPIO\n");
			goto err_set_gpio_irq;
		}
	}

	if (bdata->power_gpio >= 0) {
		retval = syna_tcm_set_gpio(tcm_hcd, bdata->power_gpio,
				true, 1, !bdata->power_on_state);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to configure power GPIO\n");
			goto err_set_gpio_power;
		}
	}

	if (bdata->reset_gpio >= 0) {
		retval = syna_tcm_set_gpio(tcm_hcd, bdata->reset_gpio,
				true, 1, !bdata->reset_on_state);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to configure reset GPIO\n");
			goto err_set_gpio_reset;
		}
	}

	if (bdata->power_gpio >= 0) {
		gpio_set_value(bdata->power_gpio, bdata->power_on_state);
		msleep(bdata->power_delay_ms);
	}

	if (bdata->reset_gpio >= 0) {
		gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
		msleep(bdata->reset_active_ms);
		gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
		msleep(bdata->reset_delay_ms);
	}

	return 0;

err_set_gpio_reset:
	if (bdata->power_gpio >= 0)
		syna_tcm_set_gpio(tcm_hcd, bdata->power_gpio, false, 0, 0);

err_set_gpio_power:
	if (bdata->irq_gpio >= 0)
		syna_tcm_set_gpio(tcm_hcd, bdata->irq_gpio, false, 0, 0);

err_set_gpio_irq:
	return retval;
}

static int syna_tcm_enable_regulator(struct syna_tcm_hcd *tcm_hcd, bool on)
{
	int ret = 0;
	LOGE(tcm_hcd->pdev->dev.parent, "syna_tcm_enable_regulator\n");

	if (on) {
		ret = regulator_enable(tcm_hcd->avdd);
		if (ret) {
			LOGE(tcm_hcd->pdev->dev.parent, "Failed to enable avdd:%d", ret);
			return ret;
		}
		usleep_range(3000, 3100);
		ret = regulator_enable(tcm_hcd->iovdd);
		if (ret) {
			regulator_disable(tcm_hcd->avdd);
			LOGE(tcm_hcd->pdev->dev.parent, "Failed to enable iovdd:%d", ret);
			return ret;
		}

		return 0;
	}

	/*power off process */
	ret = regulator_disable(tcm_hcd->iovdd);
	if (ret)
		LOGE(tcm_hcd->pdev->dev.parent, "Failed to disable iovdd:%d", ret);

	ret = regulator_disable(tcm_hcd->avdd);
	if (!ret)
		LOGE(tcm_hcd->pdev->dev.parent, "regulator disable SUCCESS");
	else
		LOGE(tcm_hcd->pdev->dev.parent, "Failed to disable analog power:%d", ret);

	return ret;
}

static int syna_tcm_regulator_init(struct syna_tcm_hcd *tcm_hcd)
{
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	struct device *dev = tcm_hcd->pdev->dev.parent;
	int ret = 0;


	LOGN(tcm_hcd->pdev->dev.parent, "Power init");
	if (strlen(bdata->avdd_name)) {
		tcm_hcd->avdd = devm_regulator_get(dev,
				bdata->avdd_name);
		if (IS_ERR_OR_NULL(tcm_hcd->avdd)) {
			ret = PTR_ERR(tcm_hcd->avdd);
			LOGE(tcm_hcd->pdev->dev.parent,"Failed to get regulator avdd:%d", ret);
			tcm_hcd->avdd = NULL;
			return ret;
		}
	} else {
		LOGE(tcm_hcd->pdev->dev.parent, "Avdd name is NULL");
	}
	ret = regulator_set_voltage(tcm_hcd->avdd, 3200000, 3200000);
	if (ret) {
		LOGE(tcm_hcd->pdev->dev.parent, "regulator_set_voltage failed %d\n", ret);
		return ret;
	}

	if (strlen(bdata->iovdd_name)) {
		tcm_hcd->iovdd = devm_regulator_get(dev,
				bdata->iovdd_name);
		if (IS_ERR_OR_NULL(tcm_hcd->iovdd)) {
			ret = PTR_ERR(tcm_hcd->iovdd);
			LOGE(tcm_hcd->pdev->dev.parent, "Failed to get regulator iovdd:%d", ret);
			tcm_hcd->iovdd = NULL;
		}
	} else {
		LOGE(tcm_hcd->pdev->dev.parent, "iovdd name is NULL");
	}
	ret = regulator_set_voltage(tcm_hcd->iovdd, 1800000, 1800000);
	if (ret) {
		LOGE(tcm_hcd->pdev->dev.parent, "regulator_set_voltage failed %d\n", ret);
		return ret;
	}

	return ret;
}

static void syna_tcm_esd_recovery(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;

	LOGI(tcm_hcd->pdev->dev.parent,	"syna_tcm_esd_recovery enter!\n");
	mutex_lock(&tcm_hcd->esd_recovery_mutex);

	/* power down */
	syna_tcm_enable_regulator(tcm_hcd, false);

	/* power up */
	retval = syna_tcm_enable_regulator(tcm_hcd, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to enable regulators, retval = %d\n", retval);
	}

	/* hardware reset */
	retval = tcm_hcd->reset_n_reinit(tcm_hcd, true, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do reset and reinit, retval = %d\n", retval);
	}

	mutex_unlock(&tcm_hcd->esd_recovery_mutex);
	return;
}

static int syna_tcm_get_app_info(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	unsigned int timeout;

	timeout = APP_STATUS_POLL_TIMEOUT_MS;

	resp_buf = NULL;
	resp_buf_size = 0;

get_app_info:
	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_APPLICATION_INFO,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_GET_APPLICATION_INFO));
		goto exit;
	}

	retval = secure_memcpy((unsigned char *)&tcm_hcd->app_info,
			sizeof(tcm_hcd->app_info),
			resp_buf,
			resp_buf_size,
			MIN(sizeof(tcm_hcd->app_info), resp_length));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy application info\n");
		goto exit;
	}

	tcm_hcd->app_status = le2_to_uint(tcm_hcd->app_info.status);

	if (tcm_hcd->app_status == APP_STATUS_BOOTING ||
			tcm_hcd->app_status == APP_STATUS_UPDATING) {
		if (timeout > 0) {
			msleep(APP_STATUS_POLL_MS);
			timeout -= APP_STATUS_POLL_MS;
			goto get_app_info;
		}
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int syna_tcm_get_boot_info(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	resp_buf = NULL;
	resp_buf_size = 0;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_BOOT_INFO,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_GET_BOOT_INFO));
		goto exit;
	}

	retval = secure_memcpy((unsigned char *)&tcm_hcd->boot_info,
			sizeof(tcm_hcd->boot_info),
			resp_buf,
			resp_buf_size,
			MIN(sizeof(tcm_hcd->boot_info), resp_length));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy boot info\n");
		goto exit;
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int syna_tcm_get_romboot_info(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	resp_buf = NULL;
	resp_buf_size = 0;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_ROMBOOT_INFO,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_GET_ROMBOOT_INFO));
		goto exit;
	}

	retval = secure_memcpy((unsigned char *)&tcm_hcd->romboot_info,
			sizeof(tcm_hcd->romboot_info),
			resp_buf,
			resp_buf_size,
			MIN(sizeof(tcm_hcd->romboot_info), resp_length));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy boot info\n");
		goto exit;
	}

	LOGD(tcm_hcd->pdev->dev.parent,
			"version = %d\n", tcm_hcd->romboot_info.version);

	LOGD(tcm_hcd->pdev->dev.parent,
			"status = 0x%02x\n", tcm_hcd->romboot_info.status);

	LOGD(tcm_hcd->pdev->dev.parent,
			"version = 0x%02x 0x%02x\n",
			tcm_hcd->romboot_info.asic_id[0],
			tcm_hcd->romboot_info.asic_id[1]);

	LOGD(tcm_hcd->pdev->dev.parent,
			"write_block_size_words = %d\n",
			tcm_hcd->romboot_info.write_block_size_words);

	LOGD(tcm_hcd->pdev->dev.parent,
			"max_write_payload_size = %d\n",
			tcm_hcd->romboot_info.max_write_payload_size[0] |
			tcm_hcd->romboot_info.max_write_payload_size[1] << 8);

	LOGD(tcm_hcd->pdev->dev.parent,
			"last_reset_reason = 0x%02x\n",
			tcm_hcd->romboot_info.last_reset_reason);

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int syna_tcm_identify(struct syna_tcm_hcd *tcm_hcd, bool id)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	unsigned int max_write_size;

	resp_buf = NULL;
	resp_buf_size = 0;

	mutex_lock(&tcm_hcd->identify_mutex);

	if (!id)
		goto get_info;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_IDENTIFY,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_IDENTIFY));
		goto exit;
	}

	retval = secure_memcpy((unsigned char *)&tcm_hcd->id_info,
			sizeof(tcm_hcd->id_info),
			resp_buf,
			resp_buf_size,
			MIN(sizeof(tcm_hcd->id_info), resp_length));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy identification info\n");
		goto exit;
	}

	tcm_hcd->packrat_number = le4_to_uint(tcm_hcd->id_info.build_id);

	max_write_size = le2_to_uint(tcm_hcd->id_info.max_write_size);
	tcm_hcd->wr_chunk_size = MIN(max_write_size, WR_CHUNK_SIZE);
	if (tcm_hcd->wr_chunk_size == 0)
		tcm_hcd->wr_chunk_size = max_write_size;

	LOGN(tcm_hcd->pdev->dev.parent,
		"Firmware build id = %d\n", tcm_hcd->packrat_number);

get_info:
	switch (tcm_hcd->id_info.mode) {
	case MODE_APPLICATION_FIRMWARE:
	case MODE_HOSTDOWNLOAD_FIRMWARE:

		retval = syna_tcm_get_app_info(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to get application info\n");
			goto exit;
		}
		break;
	case MODE_BOOTLOADER:
	case MODE_TDDI_BOOTLOADER:

		LOGD(tcm_hcd->pdev->dev.parent,
			"In bootloader mode\n");

		retval = syna_tcm_get_boot_info(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to get boot info\n");
			goto exit;
		}
		break;
	case MODE_ROMBOOTLOADER:

		LOGD(tcm_hcd->pdev->dev.parent,
			"In rombootloader mode\n");

		retval = syna_tcm_get_romboot_info(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to get application info\n");
			goto exit;
		}
		break;
	default:
		break;
	}

	retval = 0;

exit:
	mutex_unlock(&tcm_hcd->identify_mutex);

	kfree(resp_buf);

	return retval;
}

static int syna_tcm_run_production_test_firmware(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	bool retry;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	retry = true;

	resp_buf = NULL;
	resp_buf_size = 0;

retry:
	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_ENTER_PRODUCTION_TEST_MODE,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			MODE_SWITCH_DELAY_MS);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_ENTER_PRODUCTION_TEST_MODE));
		goto exit;
	}

	if (tcm_hcd->id_info.mode != MODE_PRODUCTIONTEST_FIRMWARE) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run production test firmware\n");
		if (retry) {
			retry = false;
			goto retry;
		}
		retval = -EINVAL;
		goto exit;
	} else if (tcm_hcd->app_status != APP_STATUS_OK) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Application status = 0x%02x\n",
				tcm_hcd->app_status);
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int syna_tcm_run_application_firmware(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	bool retry;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	retry = true;

	resp_buf = NULL;
	resp_buf_size = 0;

retry:
	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_RUN_APPLICATION_FIRMWARE,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			MODE_SWITCH_DELAY_MS);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_RUN_APPLICATION_FIRMWARE));
		goto exit;
	}

	retval = tcm_hcd->identify(tcm_hcd, false);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification\n");
		goto exit;
	}

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode)) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run application firmware (boot status = 0x%02x)\n",
				tcm_hcd->boot_info.status);
		if (retry) {
			retry = false;
			goto retry;
		}
		retval = -EINVAL;
		goto exit;
	} else if (tcm_hcd->app_status != APP_STATUS_OK) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Application status = 0x%02x\n",
				tcm_hcd->app_status);
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int syna_tcm_run_bootloader_firmware(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	unsigned char command;

	resp_buf = NULL;
	resp_buf_size = 0;
	command = (tcm_hcd->id_info.mode == MODE_ROMBOOTLOADER) ?
			CMD_ROMBOOT_RUN_BOOTLOADER_FIRMWARE :
			CMD_RUN_BOOTLOADER_FIRMWARE;

	retval = tcm_hcd->write_message(tcm_hcd,
			command,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			MODE_SWITCH_DELAY_MS);
	if (retval < 0) {
		if (tcm_hcd->id_info.mode == MODE_ROMBOOTLOADER) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_ROMBOOT_RUN_BOOTLOADER_FIRMWARE));
		} else {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_RUN_BOOTLOADER_FIRMWARE));
		}
		goto exit;
	}

	if (command != CMD_ROMBOOT_RUN_BOOTLOADER_FIRMWARE) {
		retval = tcm_hcd->identify(tcm_hcd, false);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification\n");
		goto exit;
		}

		if (IS_FW_MODE(tcm_hcd->id_info.mode)) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enter bootloader mode\n");
			retval = -EINVAL;
			goto exit;
		}
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int syna_tcm_switch_mode(struct syna_tcm_hcd *tcm_hcd,
		enum firmware_mode mode)
{
	int retval;

	mutex_lock(&tcm_hcd->reset_mutex);

#ifdef WATCHDOG_SW
	tcm_hcd->update_watchdog(tcm_hcd, false);
#endif

	switch (mode) {
	case FW_MODE_BOOTLOADER:
		retval = syna_tcm_run_bootloader_firmware(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to switch to bootloader mode\n");
			goto exit;
		}
		break;
	case FW_MODE_APPLICATION:
		retval = syna_tcm_run_application_firmware(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to switch to application mode\n");
			goto exit;
		}
		break;
	case FW_MODE_PRODUCTION_TEST:
		retval = syna_tcm_run_production_test_firmware(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to switch to production test mode\n");
			goto exit;
		}
		break;
	default:
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid firmware mode\n");
		retval = -EINVAL;
		goto exit;
	}

	retval = 0;

exit:
#ifdef WATCHDOG_SW
	tcm_hcd->update_watchdog(tcm_hcd, true);
#endif

	mutex_unlock(&tcm_hcd->reset_mutex);

	return retval;
}

static int syna_tcm_get_dynamic_config(struct syna_tcm_hcd *tcm_hcd,
		enum dynamic_config_id id, unsigned short *value)
{
	int retval;
	unsigned char out_buf;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	resp_buf = NULL;
	resp_buf_size = 0;

	out_buf = (unsigned char)id;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_DYNAMIC_CONFIG,
			&out_buf,
			sizeof(out_buf),
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s, id = %d\n",
				STR(CMD_GET_DYNAMIC_CONFIG), id);
		goto exit;
	}

	if (resp_length < 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid data length\n");
		retval = -EINVAL;
		goto exit;
	}

	*value = (unsigned short)le2_to_uint(resp_buf);

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int syna_tcm_set_dynamic_config(struct syna_tcm_hcd *tcm_hcd,
		enum dynamic_config_id id, unsigned short value)
{
	int retval;
	unsigned char out_buf[3];
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	resp_buf = NULL;
	resp_buf_size = 0;

	out_buf[0] = (unsigned char)id;
	out_buf[1] = (unsigned char)value;
	out_buf[2] = (unsigned char)(value >> 8);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_SET_DYNAMIC_CONFIG,
			out_buf,
			sizeof(out_buf),
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_SET_DYNAMIC_CONFIG));
		goto exit;
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static void syna_tcm_set_charge_status(void)
{
	int retval = 0;
	unsigned short val = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	val = tcm_hcd->charger_connected & 0x01; /* Default Value: 0, disconnected: 1, connected */
	if (tcm_hcd->in_sleep) {
		LOGI(tcm_hcd->pdev->dev.parent,"in sleep, don't set charge bit [%d]\n", val);
		goto exit;
	}

	/* set dynamic config */
	LOGI(tcm_hcd->pdev->dev.parent,"set charger_connected, value = 0x%02x\n", val);
	retval = tcm_hcd->set_dynamic_config(tcm_hcd, DC_CHARGER_CONNECTED, val);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"set charger_connected failed\n");
	}

exit:
	return;
}

static int syna_tcm_get_data_location(struct syna_tcm_hcd *tcm_hcd,
		enum flash_area area, unsigned int *addr, unsigned int *length)
{
	int retval;
	unsigned char out_buf;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	switch (area) {
	case CUSTOM_LCM:
		out_buf = LCM_DATA;
		break;
	case CUSTOM_OEM:
		out_buf = OEM_DATA;
		break;
	case PPDT:
		out_buf = PPDT_DATA;
		break;
	default:
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid flash area\n");
		return -EINVAL;
	}

	resp_buf = NULL;
	resp_buf_size = 0;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_DATA_LOCATION,
			&out_buf,
			sizeof(out_buf),
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_GET_DATA_LOCATION));
		goto exit;
	}

	if (resp_length != 4) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid data length\n");
		retval = -EINVAL;
		goto exit;
	}

	*addr = le2_to_uint(&resp_buf[0]);
	*length = le2_to_uint(&resp_buf[2]);

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int syna_tcm_sleep(struct syna_tcm_hcd *tcm_hcd, bool en)
{
	int retval;
	unsigned char command;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	command = en ? CMD_ENTER_DEEP_SLEEP : CMD_EXIT_DEEP_SLEEP;

	resp_buf = NULL;
	resp_buf_size = 0;

	retval = tcm_hcd->write_message(tcm_hcd,
			command,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				en ?
				STR(CMD_ENTER_DEEP_SLEEP) :
				STR(CMD_EXIT_DEEP_SLEEP));
		goto exit;
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int syna_tcm_reset(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	retval = tcm_hcd->write_message(tcm_hcd,
				CMD_RESET,
				NULL,
				0,
				&resp_buf,
				&resp_buf_size,
				&resp_length,
				NULL,
				bdata->reset_delay_ms);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_RESET));
	}

	return retval;
}

static int syna_tcm_reset_and_reinit(struct syna_tcm_hcd *tcm_hcd,
		bool hw, bool update_wd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	struct syna_tcm_module_handler *mod_handler;
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	resp_buf = NULL;
	resp_buf_size = 0;

	mutex_lock(&tcm_hcd->reset_mutex);

#ifdef WATCHDOG_SW
	if (update_wd)
		tcm_hcd->update_watchdog(tcm_hcd, false);
#endif

	if (hw) {
		if (bdata->reset_gpio < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Hardware reset unavailable\n");
			retval = -EINVAL;
			goto exit;
		}
		gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
		msleep(bdata->reset_active_ms);
		gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
	} else {
		retval = syna_tcm_reset(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
			goto exit;
		}
	}

	/* for hdl, the remaining re-init process will be done */
	/* in the helper thread, so wait for the completion here */
	if (tcm_hcd->in_hdl_mode) {
		mutex_unlock(&tcm_hcd->reset_mutex);
		kfree(resp_buf);

		retval = syna_tcm_wait_hdl(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to wait for completion of host download\n");
			return retval;
		}

#ifdef WATCHDOG_SW
		if (update_wd)
			tcm_hcd->update_watchdog(tcm_hcd, true);
#endif
		return 0;
	}

	msleep(bdata->reset_delay_ms);

	retval = tcm_hcd->identify(tcm_hcd, false);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification\n");
		goto exit;
	}

	if (IS_FW_MODE(tcm_hcd->id_info.mode))
		goto get_features;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_RUN_APPLICATION_FIRMWARE,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			MODE_SWITCH_DELAY_MS);
	if (retval < 0) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_RUN_APPLICATION_FIRMWARE));
	}

	retval = tcm_hcd->identify(tcm_hcd, false);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification\n");
		goto exit;
	}

get_features:
	LOGN(tcm_hcd->pdev->dev.parent,
			"Firmware mode = 0x%02x\n",
			tcm_hcd->id_info.mode);

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode)) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Boot status = 0x%02x\n",
				tcm_hcd->boot_info.status);
	} else if (tcm_hcd->app_status != APP_STATUS_OK) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Application status = 0x%02x\n",
				tcm_hcd->app_status);
	}

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode))
		goto dispatch_reinit;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_FEATURES,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_GET_FEATURES));
	} else {
		retval = secure_memcpy((unsigned char *)&tcm_hcd->features,
				sizeof(tcm_hcd->features),
				resp_buf,
				resp_buf_size,
				MIN(sizeof(tcm_hcd->features), resp_length));
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy feature description\n");
		}
	}

dispatch_reinit:
	mutex_lock(&mod_pool.mutex);

	if (!list_empty(&mod_pool.list)) {
		list_for_each_entry(mod_handler, &mod_pool.list, link) {
			if (!mod_handler->insert &&
					!mod_handler->detach &&
					(mod_handler->mod_cb->reinit))
				mod_handler->mod_cb->reinit(tcm_hcd);
		}
	}

	mutex_unlock(&mod_pool.mutex);

	retval = 0;

exit:
#ifdef WATCHDOG_SW
	if (update_wd)
		tcm_hcd->update_watchdog(tcm_hcd, true);
#endif

	mutex_unlock(&tcm_hcd->reset_mutex);

	kfree(resp_buf);

	return retval;
}

static int syna_tcm_rezero(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	resp_buf = NULL;
	resp_buf_size = 0;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_REZERO,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_REZERO));
		goto exit;
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static void syna_tcm_helper_work(struct work_struct *work)
{
	int retval;
	unsigned char task;
	struct syna_tcm_module_handler *mod_handler;
	struct syna_tcm_helper *helper =
			container_of(work, struct syna_tcm_helper, work);
	struct syna_tcm_hcd *tcm_hcd =
			container_of(helper, struct syna_tcm_hcd, helper);

	task = atomic_read(&helper->task);

	switch (task) {

	/* this helper can help to run the application firmware */
	case HELP_RUN_APPLICATION_FIRMWARE:
		mutex_lock(&tcm_hcd->reset_mutex);

#ifdef WATCHDOG_SW
		tcm_hcd->update_watchdog(tcm_hcd, false);
#endif
		retval = syna_tcm_run_application_firmware(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to switch to application mode\n");
		}
#ifdef WATCHDOG_SW
		tcm_hcd->update_watchdog(tcm_hcd, true);
#endif
		mutex_unlock(&tcm_hcd->reset_mutex);
		break;

	/* the reinit helper is used to notify all installed modules to */
	/* do the re-initialization process, since the HDL is completed */
	case HELP_SEND_REINIT_NOTIFICATION:
		mutex_lock(&tcm_hcd->reset_mutex);

		/* do identify to ensure application firmware is running */
		retval = tcm_hcd->identify(tcm_hcd, true);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Application firmware is not running\n");
			mutex_unlock(&tcm_hcd->reset_mutex);
			break;
		}

		/* init the touch reporting here */
		/* since the HDL is completed */
		retval = touch_reinit(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to initialze touch reporting\n");
			break;
		}

		mutex_lock(&mod_pool.mutex);
		if (!list_empty(&mod_pool.list)) {
			list_for_each_entry(mod_handler, &mod_pool.list, link) {
				if (!mod_handler->insert &&
						!mod_handler->detach &&
						(mod_handler->mod_cb->reinit))
					mod_handler->mod_cb->reinit(tcm_hcd);
			}
		}
		mutex_unlock(&mod_pool.mutex);
		mutex_unlock(&tcm_hcd->reset_mutex);
		wake_up_interruptible(&tcm_hcd->hdl_wq);
		break;

	/* this helper is used to reinit the touch reporting */
	case HELP_TOUCH_REINIT:
		retval = touch_reinit(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to re-initialze touch reporting\n");
		}
#ifdef CONFIG_FACTORY_BUILD
		syna_tcm_set_cur_value(Touch_Fod_Enable, FOD_STATUS_UNLOCKING);
#endif

		/* resend the charger bit after reset */
		syna_tcm_set_charge_status();

		/* reinit mode parameters after IC reset, to avoid losting mode setting in FW */
		syna_tcm_reinit_mode();
		break;

	/* this helper is used to trigger a romboot hdl */
	case HELP_SEND_ROMBOOT_HDL:
		syna_tcm_check_hdl(tcm_hcd, REPORT_HDL_ROMBOOT);
		break;
	default:
		break;
	}

	atomic_set(&helper->task, HELP_NONE);

	return;
}

#if defined(CONFIG_PM) || defined(CONFIG_FB)
static int syna_tcm_pm_resume(struct device *dev)
{
	int retval;
	struct syna_tcm_module_handler *mod_handler;
	struct syna_tcm_hcd *tcm_hcd = dev_get_drvdata(dev);

	LOGN(tcm_hcd->pdev->dev.parent, "-----enter-----%s\n", __func__);

	if (!tcm_hcd->in_suspend){
		LOGN(tcm_hcd->pdev->dev.parent, "tp is in resume state,-----exit-----%s\n", __func__);
		return 0;
	}

	tcm_hcd->in_suspending = false;

	if (tcm_hcd->in_hdl_mode) {
		if (!tcm_hcd->wakeup_gesture_enabled) {
			tcm_hcd->enable_irq(tcm_hcd, true, NULL);
			retval = syna_tcm_wait_hdl(tcm_hcd);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to wait for completion of host download\n");
				goto exit;
			}
			goto mod_resume;
		}
	} else {
		if (!tcm_hcd->wakeup_gesture_enabled || tcm_hcd->in_sleep)
			tcm_hcd->enable_irq(tcm_hcd, true, NULL);

#ifdef RESET_ON_RESUME
		msleep(RESET_ON_RESUME_DELAY_MS);
		goto do_reset;
#endif
	}

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode) ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Identifying mode = 0x%02x\n",
				tcm_hcd->id_info.mode);
		goto do_reset;
	}

	retval = tcm_hcd->sleep(tcm_hcd, false);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to exit deep sleep\n");
		goto exit;
	}

	if ((tcm_hcd->fod_enabled) && (tcm_hcd->fod_finger))
		goto mod_resume;

	retval = syna_tcm_rezero(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to rezero\n");
		goto exit;
	}

	goto mod_resume;

do_reset:
	retval = tcm_hcd->reset_n_reinit(tcm_hcd, false, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do reset and reinit\n");
		goto exit;
	}

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode) ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Identifying mode = 0x%02x\n",
				tcm_hcd->id_info.mode);
		retval = 0;
		goto exit;
	}

mod_resume:
	touch_resume(tcm_hcd);

#ifdef WATCHDOG_SW
	tcm_hcd->update_watchdog(tcm_hcd, true);
#endif

	mutex_lock(&mod_pool.mutex);

	if (!list_empty(&mod_pool.list)) {
		list_for_each_entry(mod_handler, &mod_pool.list, link) {
			if (!mod_handler->insert &&
					!mod_handler->detach &&
					(mod_handler->mod_cb->resume))
				mod_handler->mod_cb->resume(tcm_hcd);
		}
	}

	mutex_unlock(&mod_pool.mutex);

	retval = 0;

exit:
	tcm_hcd->in_sleep = false;
	tcm_hcd->in_suspend = false;

	return retval;
}

static int syna_tcm_pm_suspend(struct device *dev)
{
	struct syna_tcm_module_handler *mod_handler;
	struct syna_tcm_hcd *tcm_hcd = dev_get_drvdata(dev);

	LOGN(tcm_hcd->pdev->dev.parent, "-----enter-----%s\n", __func__);
	if (tcm_hcd->in_suspend){
		LOGN(tcm_hcd->pdev->dev.parent, "tp is in suspend state-----exit-----%s\n", __func__);
		return 0;
	}

	touch_suspend(tcm_hcd);

	mutex_lock(&mod_pool.mutex);

	if (!list_empty(&mod_pool.list)) {
		list_for_each_entry(mod_handler, &mod_pool.list, link) {
			if (!mod_handler->insert &&
					!mod_handler->detach &&
					(mod_handler->mod_cb->suspend))
				mod_handler->mod_cb->suspend(tcm_hcd);
		}
	}

	mutex_unlock(&mod_pool.mutex);

	if (!tcm_hcd->wakeup_gesture_enabled)
		tcm_hcd->enable_irq(tcm_hcd, false, true);

	tcm_hcd->in_suspend = true;
	touch_free_objects(tcm_hcd);

	/* Workaround to disappear ghost pointer */
	/* touch_flush_slots(tcm_hcd); */

	tcm_hcd->in_suspending = false;

	return 0;
}
#endif

static int syna_tcm_early_suspend(struct device *dev)
{
	int retval;
	struct syna_tcm_module_handler *mod_handler;
	struct syna_tcm_hcd *tcm_hcd = dev_get_drvdata(dev);

	LOGN(tcm_hcd->pdev->dev.parent, "-----enter-----%s\n", __func__);
	if (tcm_hcd->in_suspend){
		LOGE(tcm_hcd->pdev->dev.parent,"enter:syna_tcm_early_suspend------1\n");
		return 0;
	}

	tcm_hcd->in_suspending = true;

	LOGN(tcm_hcd->pdev->dev.parent,"enter:syna_tcm_early_suspend 2\n");

#ifdef WATCHDOG_SW
	tcm_hcd->update_watchdog(tcm_hcd, false);
#endif

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode) ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Identifying mode = 0x%02x\n",
				tcm_hcd->id_info.mode);
		return 0;
	}

	if (tcm_hcd->fod_enabled == true ||
			tcm_hcd->aod_enable == true ||
			tcm_hcd->doubletap_enable == true) {
		tcm_hcd->wakeup_gesture_enabled = WAKEUP_GESTURE;
	} else
		tcm_hcd->wakeup_gesture_enabled = false;

	if (!tcm_hcd->wakeup_gesture_enabled || tcm_hcd->nonui_status == 2) {
		if (!tcm_hcd->in_sleep) {
			retval = tcm_hcd->sleep(tcm_hcd, true);
			if (retval < 0) {
				tcm_hcd->in_sleep = false;
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to enter deep sleep\n");
				return retval;
			}
			tcm_hcd->in_sleep = true;
		}
	}

	if (tcm_hcd->palm_sensor_enable) {
		tcm_hcd->palm_enable_status = 0;
		update_palm_sensor_value(tcm_hcd->palm_enable_status);
	}

	touch_early_suspend(tcm_hcd);

	mutex_lock(&mod_pool.mutex);

	if (!list_empty(&mod_pool.list)) {
		list_for_each_entry(mod_handler, &mod_pool.list, link) {
			if (!mod_handler->insert &&
					!mod_handler->detach &&
					(mod_handler->mod_cb->early_suspend)) {
				LOGE(tcm_hcd->pdev->dev.parent,"enter:syna_tcm_early_suspend------3\n");
				mod_handler->mod_cb->early_suspend(tcm_hcd);
			}
		}
	}

	mutex_unlock(&mod_pool.mutex);
	LOGN(tcm_hcd->pdev->dev.parent, "-----exit-----%s\n", __func__);
	return 0;
}

static void syna_tcm_resume_work(struct work_struct *work)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd =
			container_of(work, struct syna_tcm_hcd, resume_work);

	LOGN(tcm_hcd->pdev->dev.parent, "-----enter-----%s\n", __func__);

	retval = syna_tcm_pm_resume(&tcm_hcd->pdev->dev);
	tcm_hcd->fb_ready++;

	/* resend the charger bit after resume */
	syna_tcm_set_charge_status();

	LOGN(tcm_hcd->pdev->dev.parent, "-----exit-----%s\n", __func__);

	return;
}

static void syna_tcm_early_suspend_work(struct work_struct *work)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd =
			container_of(work, struct syna_tcm_hcd, early_suspend_work);

	syna_tcm_palm_area_change_setting(1);
	LOGN(tcm_hcd->pdev->dev.parent, "-----enter-----%s\n", __func__);

	retval = syna_tcm_early_suspend(&tcm_hcd->pdev->dev);

	LOGN(tcm_hcd->pdev->dev.parent, "-----exit-----%s\n", __func__);

	return;
}

static void syna_tcm_suspend_work(struct work_struct *work)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd =
			container_of(work, struct syna_tcm_hcd, suspend_work);

	LOGN(tcm_hcd->pdev->dev.parent, "-----enter-----%s\n", __func__);

	retval = syna_tcm_pm_suspend(&tcm_hcd->pdev->dev);
	tcm_hcd->fb_ready = 0;

	LOGN(tcm_hcd->pdev->dev.parent, "-----exit-----%s\n", __func__);

	return;
}

int syna_tcm_fb_notifier_cb(struct notifier_block *nb,
		unsigned long action, void *data)
{
	int retval;
	int transition;
	struct mi_disp_notifier *evdata = data;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	LOGD(tcm_hcd->pdev->dev.parent,
			"enter: syna_tcm_fb_notifier_cb, tcm_hcd = %p\n", tcm_hcd);

	retval = 0;
	if (evdata && evdata->data && tcm_hcd) {
		transition = *(int *)(evdata->data);
		if (atomic_read(&tcm_hcd->firmware_flashing) &&
				transition == MI_DISP_DPMS_POWERDOWN) {

			retval = wait_event_interruptible_timeout(
				tcm_hcd->reflash_wq,
				!atomic_read(&tcm_hcd->firmware_flashing),
				msecs_to_jiffies(RESPONSE_TIMEOUT_MS)
				);
			if (retval == 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Timed out waiting for completion of flashing firmware\n");
				atomic_set(&tcm_hcd->firmware_flashing, 0);
				return -EIO;
			} else {
				retval = 0;
			}
		}

		flush_workqueue(tcm_hcd->event_wq);
		if (action == MI_DISP_DPMS_EARLY_EVENT && (transition == MI_DISP_DPMS_POWERDOWN)) {
			LOGN(tcm_hcd->pdev->dev.parent, "touch early_suspend by 0x%04x\n", transition);
			queue_work(tcm_hcd->event_wq, &tcm_hcd->early_suspend_work);
		} else if (action == MI_DISP_DPMS_EVENT) {
				if (transition == MI_DISP_DPMS_POWERDOWN) {
					LOGN(tcm_hcd->pdev->dev.parent,
							"touch suspend by 0x%04x\n", transition);
					queue_work(tcm_hcd->event_wq, &tcm_hcd->suspend_work);
				} else if (transition == MI_DISP_DPMS_ON) {
					LOGN(tcm_hcd->pdev->dev.parent,
							"touch resume\n");
					queue_work(tcm_hcd->event_wq, &tcm_hcd->resume_work);
				}
		}
	}
	return 0;
}

static struct notifier_block syna_tcm_noti_block = {
	.notifier_call = syna_tcm_fb_notifier_cb,
};

#ifdef SYNA_TCM_XIAOMI_TOUCHFEATURE
static struct xiaomi_touch_interface *p_xiaomi_touch_interfaces = NULL;
static int syna_tcm_xiaomi_touchfeature_exit(struct syna_tcm_hcd *tcm_hcd)
{
	cancel_work_sync(&tcm_hcd->cmd_update_work);
	cancel_work_sync(&tcm_hcd->grip_mode_work);
	flush_workqueue(tcm_hcd->game_wq);
	destroy_workqueue(tcm_hcd->game_wq);

	kfree(p_xiaomi_touch_interfaces);
	p_xiaomi_touch_interfaces = NULL;

	return 0;
}

/* TODO: this function can be called to set the report rate to FW */
static void syna_tcm_set_report_rate_work(struct work_struct *work)
{
	int retval = 0;
	unsigned short val = 0;
	struct syna_tcm_hcd *tcm_hcd =
			container_of(work, struct syna_tcm_hcd, set_report_rate_work);

	/* set dynamic config */
	val = tcm_hcd->report_rate_mode & 0x03; /* 0:default(180Hz), 1:240Hz 2: 180Hz,3:120Hz */
	LOGI(tcm_hcd->pdev->dev.parent,"set report rate, value = 0x%02x\n", val);
	retval = tcm_hcd->set_dynamic_config(tcm_hcd, DC_SET_REPORT_RATE, val);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"set report rate failed\n");
	}

	return;
}

static void syna_tcm_fod_work(struct work_struct *work)
{
	int retval = 0;
	unsigned short val = 0;
	struct syna_tcm_hcd *tcm_hcd =
			container_of(work, struct syna_tcm_hcd, fod_work);

	val = (tcm_hcd->fod_enabled) ? 3 : 0;
	LOGI(tcm_hcd->pdev->dev.parent,"set touch and hold enable control, value = 0x%02x\n", val);
	retval = tcm_hcd->set_dynamic_config(tcm_hcd, DC_ENABLE_TOUCH_AND_HOLD, val);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"set touch and hold enable control failed\n");
	}

	return;
}

static void syna_tcm_set_grip_rect(int *buf, struct syna_grip_zone *zone)
{
	int offset = 0;
	unsigned char *out_buf = NULL;
	unsigned int type = 0, pos = 0;
	int x_start = 0, y_start = 0, x_end = 0, y_end = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	/* For grip mode, the format from framework is :
	 * mode: grip mode or other
	 * len: The num of the commond, rect_num * parameters_num_for_each_rect
	 * grip_type: dead grip, or edge grip or cornero grip
	 * grip_pos: which corner or which edge
	 * x start
	 * y start
	 * x end
	 * y end
	 * time
	 * node num
	 */
	type = *buf;
	pos = *(buf + 1);
	x_start = *(buf + 2);
	y_start = *(buf + 3);
	x_end = *(buf + 4);
	y_end = *(buf + 5);
	LOGD(tcm_hcd->pdev->dev.parent,
		"grip_type: %d, grip_pos: %d, x_start: %d, y_start: %d, x_end: %d, y_end: %d\n",
		type, pos, x_start, y_start, x_end, y_end);

	if ((type > 2) || (pos > 3)){
		LOGE(tcm_hcd->pdev->dev.parent,
			"incorrect value, type:%d, pos:%d\n", type, pos);
		goto exit;
	}

	switch(type) {
		case DEAD_ZONE:
			out_buf = (unsigned char *)(&zone->dead_zone.x0[0]);
			break;
		case EDGE_ZONE:
			out_buf = (unsigned char *)(&zone->edge_zone.x0[0]);
			break;
		case CORNER_ZONE:
			out_buf = (unsigned char *)(&zone->corner_zone.x0[0]);
			break;
		default:
			out_buf = NULL;
			break;
	}

	if (!out_buf)
		goto exit;

	offset = pos*8;

	out_buf[offset + 0] = (unsigned char)(x_start & 0xff);
	out_buf[offset + 1] = (unsigned char)((x_start >> 8) & 0xff);
	out_buf[offset + 2] = (unsigned char)(y_start & 0xff);
	out_buf[offset + 3] = (unsigned char)((y_start >> 8) & 0xff);
	out_buf[offset + 4] = (unsigned char)(x_end & 0xff);
	out_buf[offset + 5] = (unsigned char)((x_end >> 8) & 0xff);
	out_buf[offset + 6] = (unsigned char)(y_end & 0xff);
	out_buf[offset + 7] = (unsigned char)((y_end >> 8) & 0xff);

exit:

	return;
}

static void syna_tcm_deadzone_rejection(bool on, int direction, struct syna_grip_zone *zone)
{
	int i = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;
	const struct syna_tcm_xiaomi_board_data *bdata = &tcm_hcd->xiaomi_board_data;
#ifdef GRIP_MODE_DEBUG
	int *pVal;

	pVal = (int *)&(bdata->deadzone_filter_hor[0]);
	LOGI(tcm_hcd->pdev->dev.parent, "deadzone hor %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n",
						pVal[0], pVal[1], pVal[2], pVal[3], pVal[4], pVal[5], pVal[6], pVal[7], pVal[8], pVal[9], pVal[10], pVal[11],
						pVal[12], pVal[13], pVal[14], pVal[15], pVal[16], pVal[17], pVal[18], pVal[19], pVal[20], pVal[21], pVal[22], pVal[23],
						pVal[24], pVal[25], pVal[26], pVal[27], pVal[28], pVal[29], pVal[30], pVal[31]);
	pVal = (int *)&(bdata->deadzone_filter_ver[0]);
	LOGI(tcm_hcd->pdev->dev.parent, "deadzone ver %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n",
						pVal[0], pVal[1], pVal[2], pVal[3], pVal[4], pVal[5], pVal[6], pVal[7], pVal[8], pVal[9], pVal[10], pVal[11],
						pVal[12], pVal[13], pVal[14], pVal[15], pVal[16], pVal[17], pVal[18], pVal[19], pVal[20], pVal[21], pVal[22], pVal[23],
						pVal[24], pVal[25], pVal[26], pVal[27], pVal[28], pVal[29], pVal[30], pVal[31]);
#endif
	if (direction) {
		for (i = 0; i < GRIP_RECT_NUM * GRIP_PARAMETER_NUM / 3; i += GRIP_PARAMETER_NUM) {
			syna_tcm_set_grip_rect((int *)&(bdata->deadzone_filter_hor[i]), zone);
		}
	} else {
		for (i = 0; i < GRIP_RECT_NUM * GRIP_PARAMETER_NUM / 3; i += GRIP_PARAMETER_NUM) {
			syna_tcm_set_grip_rect((int *)&(bdata->deadzone_filter_ver[i]), zone);
		}
	}

	return;
}

static void syna_tcm_edge_rejection(bool on, int direction, struct syna_grip_zone *zone)
{
	int i = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;
	const struct syna_tcm_xiaomi_board_data *bdata = &tcm_hcd->xiaomi_board_data;
#ifdef GRIP_MODE_DEBUG
	int *pVal;

	pVal = (int *)&(bdata->edgezone_filter_hor[0]);
	LOGI(tcm_hcd->pdev->dev.parent, "Edge hor %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n",
						pVal[0], pVal[1], pVal[2], pVal[3], pVal[4], pVal[5], pVal[6], pVal[7], pVal[8], pVal[9], pVal[10], pVal[11],
						pVal[12], pVal[13], pVal[14], pVal[15], pVal[16], pVal[17], pVal[18], pVal[19], pVal[20], pVal[21], pVal[22], pVal[23],
						pVal[24], pVal[25], pVal[26], pVal[27], pVal[28], pVal[29], pVal[30], pVal[31]);

	pVal = (int *)&(bdata->edgezone_filter_ver[0]);
	LOGI(tcm_hcd->pdev->dev.parent, "Edge ver %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n",
						pVal[0], pVal[1], pVal[2], pVal[3], pVal[4], pVal[5], pVal[6], pVal[7], pVal[8], pVal[9], pVal[10], pVal[11],
						pVal[12], pVal[13], pVal[14], pVal[15], pVal[16], pVal[17], pVal[18], pVal[19], pVal[20], pVal[21], pVal[22], pVal[23],
						pVal[24], pVal[25], pVal[26], pVal[27], pVal[28], pVal[29], pVal[30], pVal[31]);
#endif

	if (direction) {
		for (i = 0; i < GRIP_RECT_NUM * GRIP_PARAMETER_NUM / 3; i += GRIP_PARAMETER_NUM) {
			syna_tcm_set_grip_rect((int *)&(bdata->edgezone_filter_hor[i]), zone);
		}
	} else {
		for (i = 0; i < GRIP_RECT_NUM * GRIP_PARAMETER_NUM / 3; i += GRIP_PARAMETER_NUM) {
			syna_tcm_set_grip_rect((int *)&(bdata->edgezone_filter_ver[i]), zone);
		}
	}

	return;
}

static void syna_tcm_corner_rejection(bool on, int direction, struct syna_grip_zone *zone)
{
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;
	struct syna_tcm_xiaomi_board_data *bdata = &tcm_hcd->xiaomi_board_data;
	int filter_value = 0, i = 0;
	int corner_filter[GRIP_RECT_NUM * GRIP_PARAMETER_NUM / 3] = {0};
#ifdef GRIP_MODE_DEBUG
	int *pVal;
#endif

#ifdef GRIP_MODE_DEBUG
	LOGI(tcm_hcd->pdev->dev.parent,  "xiaomi x_max %d, y_max %d\n", bdata->x_max, bdata->y_max);

	pVal = (int *)&(bdata->cornerzone_filter_hor1[0]);
	LOGI(tcm_hcd->pdev->dev.parent, "Corner hor1 %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n",
		pVal[0], pVal[1], pVal[2], pVal[3], pVal[4], pVal[5], pVal[6], pVal[7], pVal[8], pVal[9], pVal[10], pVal[11],
		pVal[12], pVal[13], pVal[14], pVal[15], pVal[16], pVal[17], pVal[18], pVal[19], pVal[20], pVal[21], pVal[22], pVal[23],
		pVal[24], pVal[25], pVal[26], pVal[27], pVal[28], pVal[29], pVal[30], pVal[31]);

	pVal = (int *)&(bdata->cornerzone_filter_hor2[0]);
	LOGI(tcm_hcd->pdev->dev.parent, "Corner hor1 %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n",
		pVal[0], pVal[1], pVal[2], pVal[3], pVal[4], pVal[5], pVal[6], pVal[7], pVal[8], pVal[9], pVal[10], pVal[11],
		pVal[12], pVal[13], pVal[14], pVal[15], pVal[16], pVal[17], pVal[18], pVal[19], pVal[20], pVal[21], pVal[22], pVal[23],
		pVal[24], pVal[25], pVal[26], pVal[27], pVal[28], pVal[29], pVal[30], pVal[31]);

	pVal = (int *)&(bdata->cornerzone_filter_ver[0]);
	LOGI(tcm_hcd->pdev->dev.parent, "cornerzone_ver %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n",
		pVal[0], pVal[1], pVal[2], pVal[3], pVal[4], pVal[5], pVal[6], pVal[7], pVal[8], pVal[9], pVal[10], pVal[11],
		pVal[12], pVal[13], pVal[14], pVal[15],	pVal[16], pVal[17], pVal[18], pVal[19], pVal[20], pVal[21], pVal[22], pVal[23],
		pVal[24], pVal[25], pVal[26], pVal[27], pVal[28], pVal[29], pVal[30], pVal[31]);
#endif

	switch (p_xiaomi_touch_interfaces->touch_mode[Touch_Edge_Filter][SET_CUR_VALUE]) {
		case 0:
			filter_value = 0;
			break;
		case 1:
			filter_value = bdata->cornerfilter_area_step1;
			break;
		case 2:
			filter_value = bdata->cornerfilter_area_step2;
			break;
		case 3:
			filter_value = bdata->cornerfilter_area_step3;
			break;
		default:
			filter_value = bdata->cornerfilter_area_step2;
			LOGI(tcm_hcd->pdev->dev.parent,  "%s: no support value use default filter x/y value\n", __func__);
			break;
	}

	if (filter_value == 0 && direction != 0) {
		for (i = 0; i < GRIP_RECT_NUM * GRIP_PARAMETER_NUM / 3; i += GRIP_PARAMETER_NUM) {
			corner_filter[i] = 0;
			corner_filter[i + 1] = i / GRIP_PARAMETER_NUM;
			syna_tcm_set_grip_rect(&corner_filter[i], zone);
		}
		return;
	}

	if (direction == 1) {
		bdata->cornerzone_filter_hor1[4] = filter_value;
		bdata->cornerzone_filter_hor1[5] = filter_value;
		bdata->cornerzone_filter_hor1[GRIP_PARAMETER_NUM * 2 + 3] = bdata->y_max - filter_value - 1;
		bdata->cornerzone_filter_hor1[GRIP_PARAMETER_NUM * 2 + 4] = filter_value;
		for (i = 0; i < GRIP_RECT_NUM * GRIP_PARAMETER_NUM / 3; i += GRIP_PARAMETER_NUM)
			syna_tcm_set_grip_rect((int *)&(bdata->cornerzone_filter_hor1[i]), zone);
#ifdef GRIP_MODE_DEBUG
		pVal = (int *)&(bdata->cornerzone_filter_hor1[0]);
		LOGI(tcm_hcd->pdev->dev.parent, "cornerzone_hor1 %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n",
				pVal[0], pVal[1], pVal[2], pVal[3], pVal[4], pVal[5], pVal[6], pVal[7], pVal[8], pVal[9], pVal[10], pVal[11],
				pVal[12], pVal[13], pVal[14], pVal[15],	pVal[16], pVal[17], pVal[18], pVal[19], pVal[20], pVal[21], pVal[22], pVal[23],
				pVal[24], pVal[25], pVal[26], pVal[27], pVal[28], pVal[29], pVal[30], pVal[31]);
#endif
	} else if (direction == 3) {
		bdata->cornerzone_filter_hor2[GRIP_PARAMETER_NUM + 2] = bdata->x_max - filter_value - 1;
		bdata->cornerzone_filter_hor2[GRIP_PARAMETER_NUM + 5] = filter_value;
		bdata->cornerzone_filter_hor2[GRIP_PARAMETER_NUM * 3 + 2] = bdata->x_max - filter_value - 1;
		bdata->cornerzone_filter_hor2[GRIP_PARAMETER_NUM * 3 + 3] = bdata->y_max - filter_value - 1;
		for (i = 0; i < GRIP_RECT_NUM * GRIP_PARAMETER_NUM / 3; i += GRIP_PARAMETER_NUM)
			syna_tcm_set_grip_rect((int *)&(bdata->cornerzone_filter_hor2[i]), zone);
#ifdef GRIP_MODE_DEBUG
		pVal = (int *)&(bdata->cornerzone_filter_hor2[0]);
		LOGI(tcm_hcd->pdev->dev.parent, "cornerzone_hor2 %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n",
				pVal[0], pVal[1], pVal[2], pVal[3], pVal[4], pVal[5], pVal[6], pVal[7], pVal[8], pVal[9], pVal[10], pVal[11],
				pVal[12], pVal[13], pVal[14], pVal[15],	pVal[16], pVal[17], pVal[18], pVal[19], pVal[20], pVal[21], pVal[22], pVal[23],
				pVal[24], pVal[25], pVal[26], pVal[27], pVal[28], pVal[29], pVal[30], pVal[31]);
#endif
	} else if (direction == 0) {
		for (i = 0; i < GRIP_RECT_NUM * GRIP_PARAMETER_NUM / 3; i += GRIP_PARAMETER_NUM)
			syna_tcm_set_grip_rect((int *)&(bdata->cornerzone_filter_ver[i]), zone);
#ifdef GRIP_MODE_DEBUG
		pVal = (int *)&(bdata->cornerzone_filter_ver[0]);
		LOGI(tcm_hcd->pdev->dev.parent, "cornerzone_ver %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n",
				pVal[0], pVal[1], pVal[2], pVal[3], pVal[4], pVal[5], pVal[6], pVal[7], pVal[8], pVal[9], pVal[10], pVal[11],
				pVal[12], pVal[13], pVal[14], pVal[15],	pVal[16], pVal[17], pVal[18], pVal[19], pVal[20], pVal[21], pVal[22], pVal[23],
				pVal[24], pVal[25], pVal[26], pVal[27], pVal[28], pVal[29], pVal[30], pVal[31]);
#endif
	}

	return;
}

static void syna_tcm_update_grip_mode(void)
{
	bool gamemode_on = p_xiaomi_touch_interfaces->touch_mode[Touch_Game_Mode][GET_CUR_VALUE];
	int direction = p_xiaomi_touch_interfaces->touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE];
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;
	int retval, i;
	unsigned int length_out;
	struct syna_grip_zone *grip_zone = NULL;
	unsigned char *out_buf;
	unsigned char *resp_buf = NULL;
	unsigned int resp_buf_size = 0;
	unsigned int resp_length;
	unsigned int offset;
	unsigned short checksum = 0;

	/*
	* out_buf[0]: sub_cmd_id
	* out_buf[(length_out-2):1]: struct syna_grip_zone
	* out_buf[length_out-1]: 8bit-checksum
	*/
	length_out = 1 + sizeof(struct syna_grip_zone) + 1;
	out_buf = (unsigned char *)kzalloc(length_out, GFP_KERNEL);
	if (!out_buf) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for out_buf\n");
		goto exit1;
	}

	out_buf[0] = C7_SUB_CMD_SET_GRIP_ZONE;
	grip_zone = (struct syna_grip_zone*)&out_buf[1];
	grip_zone->type = (gamemode_on ? (1 << 4) : 0) | (direction & 0x0F);

	if (!gamemode_on) {
		mutex_lock(&tcm_hcd->long_mode_value_mutex);
		for (i = 0; i < GRIP_RECT_NUM * GRIP_PARAMETER_NUM; i += GRIP_PARAMETER_NUM) {
			syna_tcm_set_grip_rect(&(p_xiaomi_touch_interfaces->long_mode_value[i]), grip_zone);
		}
		mutex_unlock(&tcm_hcd->long_mode_value_mutex);
		goto send_cmd;
	}

	syna_tcm_deadzone_rejection(gamemode_on, direction, grip_zone);
	syna_tcm_edge_rejection(gamemode_on, direction, grip_zone);
	syna_tcm_corner_rejection(gamemode_on, direction, grip_zone);

send_cmd:
	for (i = 1; i < (length_out - 1); i++) {
		checksum += out_buf[i];
	}
	out_buf[length_out - 1] = (unsigned char)((checksum & 0xFF) + ((checksum>>8) & 0xFF));

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_MultiFunction,
			out_buf,
			length_out,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n", STR(CMD_MultiFunction));
	}

	offset = 0;
	LOGI(tcm_hcd->pdev->dev.parent, "length_out:%d, SUBID %d, gamemode|panel_orientation: %02x %02x, crc8:%d checksum: %d\n",
		length_out, out_buf[0], out_buf[1], out_buf[2], out_buf[length_out - 1], checksum);
	//offset = 3;
	for (i = 0; i < 3 * 4; i++) {
		offset = 3 + i * 8;
#ifdef GRIP_MODE_DEBUG
		LOGI(tcm_hcd->pdev->dev.parent, "%s corner[%d]:%4d %4d %4d %4d\n",
			__func__, i%4, le2_to_uint(&out_buf[offset+0]), le2_to_uint(&out_buf[offset+2]),
			le2_to_uint(&out_buf[offset+4]), le2_to_uint(&out_buf[offset+6]));
#endif
	}

	LOGN(tcm_hcd->pdev->dev.parent, "Set grip zone done\n");

exit1:
	kfree(out_buf);
	kfree(resp_buf);
	return;
}

static void syna_tcm_get_grip_mode(unsigned int *gamemode, unsigned int *panel_orientation)
{
	int retval;
	int i;
	unsigned char out_buf;
	unsigned char *resp_buf = NULL;
	unsigned int resp_buf_size = 0;
	unsigned int resp_length;
	unsigned int len, offset;
	unsigned short checksum = 0;
	unsigned char crc8 = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	out_buf = (unsigned char)C7_SUB_CMD_GET_GRIP_ZONE;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_MultiFunction,
			&out_buf,
			sizeof(out_buf),
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s to get grip setting\n", STR(CMD_MultiFunction));
		goto exit;
	}

	len = sizeof(struct syna_grip_zone) + 1;
	if (resp_length < len) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid data length: resp_length = %d, len = %d\n", resp_length, len);
		retval = -EINVAL;
		goto exit;
	}

	/*
	* resp_buf[(len-2):0]: struct syna_grip_zone
	* resp_buf[len-1]: 8bit-checksum
	*/
	for (i = 0; i < (len - 1); i++) {
		checksum += resp_buf[i];
	}
	crc8 = (unsigned char)((checksum & 0xFF) + ((checksum>>8) & 0xFF));
	if (resp_buf[len - 1] != crc8) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid checksum: crc8 = %d, checksum = %d, resp_checksum = %d\n",
				crc8, checksum, resp_buf[len - 1]);
		retval = -EINVAL;
		goto exit;
	}

	*gamemode = (resp_buf[0]&(1<<4)) ? 1 : 0;
	*panel_orientation = resp_buf[0] & 0x0F;

	offset = 0;
	LOGN(tcm_hcd->pdev->dev.parent, "length:%d, gamemode|panel_orientation: %d %d, checksum: %d\n",
		resp_length, resp_buf[0], resp_buf[1], resp_buf[len - 1]);
	for (i = 0; i < 3*4; i++) {
		offset = 2 + i*8;
#ifdef GRIP_MODE_DEBUG
		LOGI(tcm_hcd->pdev->dev.parent, "%s, corner[%d]:%4d %4d %4d %4d\n",
			__func__, i%4, le2_to_uint(&resp_buf[offset+0]), le2_to_uint(&resp_buf[offset+2]),
			le2_to_uint(&resp_buf[offset+4]), le2_to_uint(&resp_buf[offset+6]));
#endif
	}

	LOGN(tcm_hcd->pdev->dev.parent, "Get grip zone done\n");

exit:
	kfree(resp_buf);
	return;
}

static void syna_tcm_update_touchmode_data(void)
{
	int i;
	int retval = 0;
	unsigned int mode_update_flag = 0;
	unsigned int dc_id = 0;
	unsigned short temp_value = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;
	unsigned int gamemode, panel_orientation;

	mutex_lock(&tcm_hcd->cmd_update_mutex);

	for (i = 0; i < Touch_Mode_NUM; i++) {
		if (p_xiaomi_touch_interfaces->touch_mode[i][GET_CUR_VALUE] != p_xiaomi_touch_interfaces->touch_mode[i][SET_CUR_VALUE]) {
			p_xiaomi_touch_interfaces->touch_mode[i][GET_CUR_VALUE] = p_xiaomi_touch_interfaces->touch_mode[i][SET_CUR_VALUE];
			temp_value = p_xiaomi_touch_interfaces->touch_mode[i][SET_CUR_VALUE];
			mode_update_flag |= 1 << i;

			dc_id = syna_tcm_get_dc_id(i);
			if (dc_id == DC_UNKNOWN) {
				continue;
			}
			/* set value to FW for dc_id in touch_mode_dc_id_table[] */
			retval = tcm_hcd->set_dynamic_config(tcm_hcd, dc_id, temp_value);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent, "set dynamic config id:0x%02x failed\n", dc_id);
			}

			// debug: read to get the current value in FW
			retval = tcm_hcd->get_dynamic_config(tcm_hcd, dc_id, &temp_value);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,	"Failed to get dynamic config id: 0x%02x\n", dc_id);
			} else {
				LOGI(tcm_hcd->pdev->dev.parent,	"get dynamic config id: 0x%02x, value: %d\n", dc_id, temp_value);
			}
		}
	}

	if ((mode_update_flag & (1 << Touch_Game_Mode)) ||
		(tcm_hcd->gamemode_enable && (mode_update_flag & (1 << Touch_Panel_Orientation))) ||
		(tcm_hcd->gamemode_enable && (mode_update_flag & (1 << Touch_Edge_Filter)))) {
		LOGI(tcm_hcd->pdev->dev.parent,	"gamemode_enabled: %d, mode_update_flag: %d\n",tcm_hcd->gamemode_enable, mode_update_flag);
		syna_tcm_update_grip_mode();

		/* debug to get the grip mode setting */
		syna_tcm_get_grip_mode(&gamemode, &panel_orientation);
	}

	mutex_unlock(&tcm_hcd->cmd_update_mutex);
	return;
}


static void syna_tcm_cmd_update_work(struct work_struct *work)
{
	syna_tcm_update_touchmode_data();

	return;
}

static void syna_tcm_grip_mode_work(struct work_struct *work)
{
	struct syna_tcm_hcd *tcm_hcd =
			container_of(work, struct syna_tcm_hcd, grip_mode_work);
	int long_mode_len;

	mutex_lock(&tcm_hcd->long_mode_value_mutex);
	long_mode_len = p_xiaomi_touch_interfaces->long_mode_len;
	mutex_unlock(&tcm_hcd->long_mode_value_mutex);

	if (long_mode_len != GRIP_RECT_NUM * GRIP_PARAMETER_NUM) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"%s len is invalid\n", __func__);
		return;
	}

	mutex_lock(&tcm_hcd->cmd_update_mutex);
	if (tcm_hcd->gamemode_enable) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"%s is in gamemode, don't set rect!\n", __func__);
		mutex_unlock(&tcm_hcd->cmd_update_mutex);
		return;
	}

	syna_tcm_update_grip_mode();
	mutex_unlock(&tcm_hcd->cmd_update_mutex);
}

static void syna_tcm_power_status_handle(struct syna_tcm_hcd *tcm_hcd)
{
	flush_workqueue(tcm_hcd->event_wq);;

	LOGI(tcm_hcd->pdev->dev.parent,
		"power_status_handle, 0x%02x\n", tcm_hcd->power_status);
	if (tcm_hcd->power_status) {
		queue_work(tcm_hcd->event_wq, &tcm_hcd->resume_work);
	} else {
		queue_work(tcm_hcd->event_wq, &tcm_hcd->suspend_work);
	}

	return;
}

#define FLAG_FOD_DISABLE 0
#define FLAG_FOD_ENABLE 1

/* Get mode & value from TP_IC */
static int syna_tcm_read_touchmode_data(void)
{
	int retval = 0;
	unsigned int dc_id = 0;
	unsigned short get_value[Touch_Mode_NUM] = {0};
	int i;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	for (i = 0; i < Touch_Mode_NUM; i++) {
		dc_id = syna_tcm_get_dc_id(i);
		if (dc_id == DC_UNKNOWN) {
			continue;
		}

		retval = tcm_hcd->get_dynamic_config(tcm_hcd, dc_id, &get_value[i]);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,	"Failed to get dynamic config id: 0x%02x\n", dc_id);
			goto exit;
		}
		p_xiaomi_touch_interfaces->touch_mode[i][GET_CUR_VALUE] = get_value[i];
	}

	LOGI(tcm_hcd->pdev->dev.parent,
		"%s: game_mode:%d, active_mode:%d, up_threshold:%d, tolerance:%d, panel orientation:%d, Report Rate:%d\n",
		__func__, get_value[Touch_Game_Mode], get_value[Touch_Active_MODE], get_value[Touch_UP_THRESHOLD],
		get_value[Touch_Tolerance],	get_value[Touch_Panel_Orientation], get_value[Touch_Report_Rate]);

exit:
	return retval;
}

static int syna_tcm_palm_area_change_setting(int value)
{
	int retval = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	LOGI(tcm_hcd->pdev->dev.parent,
		"palm area change setting, 0x%02x\n", value);

	retval = tcm_hcd->set_dynamic_config(tcm_hcd,
		DC_PALM_AREA_CHANGE, value);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to palm area change setting\n");
		goto exit;
	}

exit:
	return retval;
}

int syna_tcm_palm_sensor_write(int value)
{
	int retval = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	LOGI(tcm_hcd->pdev->dev.parent,
		"palm_sensor_write, 0x%02x\n", value);
	tcm_hcd->palm_sensor_enable = value;

	if (!tcm_hcd->palm_sensor_enable) {
		tcm_hcd->palm_enable_status = 0;
		if (!tcm_hcd->in_suspend)
			syna_tcm_palm_area_change_setting(0);
		update_palm_sensor_value(tcm_hcd->palm_enable_status);
	}
	return retval;
}

static int syna_tcm_set_cur_value(int mode, int val)
{
	int retval = 0;
	int maxVal, minVal;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	if (!p_xiaomi_touch_interfaces) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"p_xiaomi_touch_interfaces is NULL\n");
		retval = -ENOMEM;
		goto exit;
	}

	if (mode >= Touch_Mode_NUM) {
		LOGE(tcm_hcd->pdev->dev.parent, "error mode[%d]\n", mode);
		retval = -EINVAL;
		goto exit;
	}

	if (val < 0) {
		LOGE(tcm_hcd->pdev->dev.parent, "error value [%d]\n", val);
		retval = -EINVAL;
		goto exit;
	}

	switch (mode) {
	case Touch_Doubletap_Mode:
		LOGI(tcm_hcd->pdev->dev.parent, "Touch_Doubletap_Mode value [%d]\n", val);
		/* Bit0: Double Tap; Bit13: Single Tap */
		tcm_hcd->doubletap_enable = val > 0 ? true : false;
		break;
	case Touch_Aod_Enable:
		LOGI(tcm_hcd->pdev->dev.parent, "Touch_Aod_Enable value [%d]\n", val);
		tcm_hcd->aod_enable = val > 0 ? true : false;
		break;
	case Touch_Fod_Enable:
		tcm_hcd->finger_unlock_status = -1;
		LOGI(tcm_hcd->pdev->dev.parent, "Touch_Fod_Enable value [%d]\n", val);
		if (val == FOD_STATUS_INVALID || val == FOD_STATUS_DELETED) {
			val = FLAG_FOD_DISABLE;
		} else if (val == FOD_STATUS_UNLOCKED || val == FOD_STATUS_UNLOCKING ||
			val == FOD_STATUS_INPUT_FINGERPRINT || val == FOD_STATUS_UNLOCK_FAILED) {
			tcm_hcd->finger_unlock_status = val;
			val = FLAG_FOD_ENABLE;
		}

		tcm_hcd->fod_enabled = val;
		queue_work(tcm_hcd->event_wq, &tcm_hcd->fod_work);
		break;
	case Touch_Power_Status:
		LOGI(tcm_hcd->pdev->dev.parent, "Touch_Power_Status value [%d]\n", val);
		tcm_hcd->power_status = val;
		syna_tcm_power_status_handle(tcm_hcd);
		break;
	case Touch_FodIcon_Enable:
		LOGI(tcm_hcd->pdev->dev.parent, "Touch_FodIcon_Enable value [%d]\n", val);
		tcm_hcd->fod_display_enabled = false;
		tcm_hcd->fod_icon_status = val > 0 ? true : false;
		break;
	case Touch_Nonui_Mode:
		LOGI(tcm_hcd->pdev->dev.parent, "Touch_Nonui_Mode value [%d]\n", val);
		tcm_hcd->nonui_status = val;
		switch (tcm_hcd->nonui_status) {
		case 0:
			if (tcm_hcd->in_sleep && tcm_hcd->in_suspend) {
				LOGI(tcm_hcd->pdev->dev.parent, "Exit sleep mode!\n");
				/* enable irq */
				tcm_hcd->enable_irq(tcm_hcd, true, NULL);
				retval = tcm_hcd->sleep(tcm_hcd, false);
				if (retval < 0) {
					tcm_hcd->in_sleep = true;
					LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to exit deep sleep\n");
					goto exit;
				}
				tcm_hcd->in_sleep = false;
			}

			if (tcm_hcd->fod_icon_status || tcm_hcd->aod_enable) {
				LOGI(tcm_hcd->pdev->dev.parent, "Enable single tap!\n");
				tcm_hcd->gesture_type |= (0x0001 << 13);
				retval = tcm_hcd->set_dynamic_config(tcm_hcd,
					DC_GESTURE_TYPE_ENABLE, tcm_hcd->gesture_type);
				if (retval < 0) {
					LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to enable gesture type\n");
					goto exit;
				}
			}
			break;
		case 1:
			if (tcm_hcd->fod_icon_status && !tcm_hcd->aod_enable && tcm_hcd->in_suspend) {
				LOGI(tcm_hcd->pdev->dev.parent, "Disable single tap!\n");
				tcm_hcd->gesture_type &= ~(0x0001<<13);
				retval = tcm_hcd->set_dynamic_config(tcm_hcd,
					DC_GESTURE_TYPE_ENABLE, tcm_hcd->gesture_type);
				if (retval < 0) {
					LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to disable single tap\n");
					goto exit;
				}
			}
			break;
		case 2:
			if (!tcm_hcd->in_sleep && tcm_hcd->wakeup_gesture_enabled && tcm_hcd->in_suspend) {
				LOGI(tcm_hcd->pdev->dev.parent, "Enter sleep mode!\n");
				retval = tcm_hcd->sleep(tcm_hcd, true);
				if (retval < 0) {
					tcm_hcd->in_sleep = false;
					LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to enter deep sleep\n");
					goto exit;
				}
				/* disable irq */
				tcm_hcd->enable_irq(tcm_hcd, false, true);
				tcm_hcd->in_sleep = true;
			}
			break;
		}
		break;
	case Touch_Game_Mode:
		LOGI(tcm_hcd->pdev->dev.parent, "Touch_Game_Mode value [%d]\n", val);
		tcm_hcd->gamemode_enable = val > 0 ? true : false;
		break;
	case Touch_Panel_Orientation:
		LOGI(tcm_hcd->pdev->dev.parent, "Touch_Panel_Orientation value [%d]\n", val);
		break;
	case Touch_Edge_Filter:
		LOGI(tcm_hcd->pdev->dev.parent, "Touch_Edge_Filter value [%d]\n", val);
		break;
	case Touch_UP_THRESHOLD:
		LOGI(tcm_hcd->pdev->dev.parent, "Touch_UP_THRESHOLD value [%d]\n", val);
		break;
	case Touch_Tolerance:
		LOGI(tcm_hcd->pdev->dev.parent, "Touch_Tolerance value [%d]\n", val);
		break;
	default:
		LOGI(tcm_hcd->pdev->dev.parent, "other mode type mode [%d], value [%d]\n", mode, val);
		break;
	}

	maxVal = p_xiaomi_touch_interfaces->touch_mode[mode][GET_MAX_VALUE];
	minVal = p_xiaomi_touch_interfaces->touch_mode[mode][GET_MIN_VALUE];
	p_xiaomi_touch_interfaces->touch_mode[mode][SET_CUR_VALUE] =
		(val >= maxVal) ? maxVal : ((val <= minVal) ? minVal : val);

	if (mode == Touch_Game_Mode || mode == Touch_Panel_Orientation ||
		mode == Touch_UP_THRESHOLD || mode == Touch_Tolerance ||
		mode == Touch_Edge_Filter) {
		queue_work(tcm_hcd->game_wq, &tcm_hcd->cmd_update_work);
	}

exit:
	return retval;
}

/* Normal mode grip mode setup */
static int syna_tcm_set_mode_long_value(int mode, int len, int *buf)
{
	int i = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	if (len <= 0)
		return -EIO;

	LOGE(tcm_hcd->pdev->dev.parent, "%s, mode: %d, len: %d\n",
		__func__, mode, len);

	mutex_lock(&tcm_hcd->long_mode_value_mutex);
	p_xiaomi_touch_interfaces->long_mode_len = len;
	for (i = 0; i < len; i++) {
		p_xiaomi_touch_interfaces->long_mode_value[i] = buf[i];
	}
#ifdef GRIP_MODE_DEBUG
	for (i = 0; i < len; i = i + 8) {
		LOGI(tcm_hcd->pdev->dev.parent,
				"long_mode_value[0~7] = %d, %d, %d, %d, %d, %d, %d, %d\n",
				p_xiaomi_touch_interfaces->long_mode_value[i], p_xiaomi_touch_interfaces->long_mode_value[i + 1], p_xiaomi_touch_interfaces->long_mode_value[i + 2],
				p_xiaomi_touch_interfaces->long_mode_value[i + 3], p_xiaomi_touch_interfaces->long_mode_value[i + 4], p_xiaomi_touch_interfaces->long_mode_value[i + 5],
				p_xiaomi_touch_interfaces->long_mode_value[i + 6], p_xiaomi_touch_interfaces->long_mode_value[i + 7]);
	}
#endif
	mutex_unlock(&tcm_hcd->long_mode_value_mutex);

	if (mode == Touch_Grip_Mode) {
		if (tcm_hcd->gamemode_enable) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"%s in gamemode, can't write parameters to touch ic\n",
				__func__);
			return 0;
		} else {
			schedule_work(&tcm_hcd->grip_mode_work);
		}
	}

	return 0;
}

static int syna_tcm_get_mode_value(int mode, int val_type)
{
	int val = -1;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	if (!p_xiaomi_touch_interfaces) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"p_xiaomi_touch_interfaces is NULL\n");
		return val;
	}

	if ((mode < Touch_Mode_NUM) && (mode >= 0)) {
		val = p_xiaomi_touch_interfaces->touch_mode[mode][val_type];
	}
	else
		LOGE(tcm_hcd->pdev->dev.parent, "error mode[%d]\n", mode);

	return val;
}

static int syna_tcm_get_mode_all(int mode, int *val)
{
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	if (!p_xiaomi_touch_interfaces) {
		LOGE(tcm_hcd->pdev->dev.parent,"p_xiaomi_touch_interfaces is NULL\n");
		return (-ENOMEM);
	}

	if ((mode < Touch_Mode_NUM) && (mode >= 0)) {
		val[0] = p_xiaomi_touch_interfaces->touch_mode[mode][GET_CUR_VALUE];
		val[1] = p_xiaomi_touch_interfaces->touch_mode[mode][GET_DEF_VALUE];
		val[2] = p_xiaomi_touch_interfaces->touch_mode[mode][GET_MIN_VALUE];
		val[3] = p_xiaomi_touch_interfaces->touch_mode[mode][GET_MAX_VALUE];
	}
	else {
		LOGE(tcm_hcd->pdev->dev.parent, "error mode[%d]\n", mode);
	}

	return 0;
}

static int syna_tcm_reset_mode(int mode)
{
	int i = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	if (!p_xiaomi_touch_interfaces) {
		LOGE(tcm_hcd->pdev->dev.parent,"p_xiaomi_touch_interfaces is NULL\n");
		return -ENOMEM;
	}

	if (mode < 0) {
		LOGE(tcm_hcd->pdev->dev.parent, "do not support this mode %d\n", mode);
		return -EINVAL;
	}

	LOGI(tcm_hcd->pdev->dev.parent,"reset mode %d\n", mode);
	if (mode < Touch_Mode_NUM && mode > 0) {
		p_xiaomi_touch_interfaces->touch_mode[mode][SET_CUR_VALUE] =
			p_xiaomi_touch_interfaces->touch_mode[mode][GET_DEF_VALUE];

	} else if (mode == 0) {
		for (i = 0; i < Touch_Mode_NUM; i++) {
			if (i == Touch_Panel_Orientation) {
				p_xiaomi_touch_interfaces->touch_mode[i][SET_CUR_VALUE] =
					p_xiaomi_touch_interfaces->touch_mode[i][GET_CUR_VALUE];
			} else {
				p_xiaomi_touch_interfaces->touch_mode[mode][SET_CUR_VALUE] =
					p_xiaomi_touch_interfaces->touch_mode[mode][GET_DEF_VALUE];
				p_xiaomi_touch_interfaces->touch_mode[mode][GET_CUR_VALUE] =
					p_xiaomi_touch_interfaces->touch_mode[mode][SET_CUR_VALUE];
			}
		}
		tcm_hcd->gamemode_enable = false;
	}
	queue_work(tcm_hcd->game_wq, &tcm_hcd->cmd_update_work);

	return 0;
}

static void syna_tcm_reinit_mode(void)
{
	unsigned int gamemode, panel_orientation;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	syna_tcm_get_grip_mode(&gamemode, &panel_orientation);
	p_xiaomi_touch_interfaces->touch_mode[Touch_Game_Mode][GET_CUR_VALUE] = gamemode;
	p_xiaomi_touch_interfaces->touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] = panel_orientation;
	syna_tcm_read_touchmode_data();

	queue_work(tcm_hcd->game_wq, &tcm_hcd->cmd_update_work);

	return;
}

static int syna_tcm_parse_gamemode_param_dt(struct syna_tcm_xiaomi_board_data *bdata)
{
	int retval, i, j;
	u32 temp_val[SYNA_GRIP_PARAMETERS_SIZE];
	unsigned int *pVal = NULL;
	char *str_dts = NULL;
	struct property *prop;
	struct spi_device *spi;
	struct device_node *np = NULL;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	/* parse dtsi */
	spi = to_spi_device(tcm_hcd->pdev->dev.parent);
	np = spi->dev.of_node;

	/* parse the touch mode setting in dtsi */
	for (i = 0; i < Touch_Mode_NUM; i++) {
		memset(temp_val, 0, sizeof(temp_val));
		switch (i) {
			case Touch_Game_Mode:
				pVal = bdata->game_mode;
				str_dts = SYNA_GAME_MODE_ARRAY;
				break;
			case Touch_Active_MODE:
				pVal = bdata->active_mode;
				str_dts = SYNA_ACTIVE_MODE_ARRAY;
				break;
			case Touch_UP_THRESHOLD:
				pVal = bdata->up_threshold;
				str_dts = SYNA_UP_THRESHOLD_ARRAY;
				break;
			case Touch_Tolerance:
				pVal = bdata->tolerance;
				str_dts = SYNA_TOLERANCE_ARRAY;
				break;
			case Touch_Edge_Filter:
				pVal = bdata->edge_filter;
				str_dts = SYNA_EDGE_FILTER_ARRAY;
				break;
			case Touch_Panel_Orientation:
				pVal = bdata->panel_orien;
				str_dts = SYNA_PANEL_ORIEN_ARRAY;
				break;
			case Touch_Report_Rate:
				pVal = bdata->report_rate;
				str_dts = SYNA_REPORT_RATE_ARRAY;
				break;
			default:
				pVal = NULL;
				str_dts = NULL;
				break;
		}

		if (pVal == NULL || str_dts == NULL)
			continue;

		prop = of_find_property(np, str_dts, NULL);
		if (prop && prop->length) {
			retval = of_property_read_u32_array(np, str_dts ,temp_val, SYNA_TOUCH_MODE_PARAMETERS_SIZE);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent, "Failed to read synaptics, %s property\n", str_dts);
			}
		}

		/* if there is no correct setting in dts, set it to 0 */
		for (j = 0; j < SYNA_TOUCH_MODE_PARAMETERS_SIZE; j++) {
			pVal[j] = temp_val[j];
		}
		LOGD(tcm_hcd->pdev->dev.parent, "%s: %2d %2d %2d %2d %2d\n",
			str_dts, pVal[0], pVal[1], pVal[2], pVal[3], pVal[4]);
	}

	/* parse the grip conrnerfiler_area_step */
	memset(temp_val, 0, sizeof(temp_val));
	str_dts = SYNA_CORNER_FILTER_AREA_STEP_ARRAY;
	prop = of_find_property(np, str_dts, NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32_array(np, str_dts ,temp_val, SYNA_CORNERFILTER_AREA_STEP_SIZE);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent, "Failed to read synaptics, %s property\n", str_dts);
		}
	}
	bdata->cornerfilter_area_step0 = temp_val[0];
	bdata->cornerfilter_area_step1 = temp_val[1];
	bdata->cornerfilter_area_step2 = temp_val[2];
	bdata->cornerfilter_area_step3 = temp_val[3];
	LOGI(tcm_hcd->pdev->dev.parent, "%s: %2d %2d %2d %2d\n",
			str_dts, temp_val[0], temp_val[1], temp_val[2], temp_val[3]);

	/* parse the grip cornerzone/deadzone/edgezone filter */
	for (i = 0; i < 7; i++) {
		memset(temp_val, 0, sizeof(temp_val));
		switch (i) {
			case 0:
				pVal = bdata->cornerzone_filter_hor1;
				str_dts = SYNA_CORNER_ZONE_FILTER_HOR1_ARRAY;
				break;
			case 1:
				pVal = bdata->cornerzone_filter_hor2;
				str_dts = SYNA_CORNER_ZONE_FILTER_HOR2_ARRAY;
				break;
			case 2:
				pVal = bdata->cornerzone_filter_ver;
				str_dts = SYNA_CORNER_ZONE_FILTER_VER_ARRAY;
				break;
			case 3:
				pVal = bdata->deadzone_filter_hor;
				str_dts = SYNA_DEAD_ZONE_FILTER_HOR_ARRAY;
				break;
			case 4:
				pVal = bdata->deadzone_filter_ver;
				str_dts = SYNA_DEAD_ZONE_FILTER_VER_ARRAY;
				break;
			case 5:
				pVal = bdata->edgezone_filter_hor;
				str_dts = SYNA_EDGE_ZONE_FILTER_HOR_ARRAY;
				break;
			case 6:
				pVal = bdata->edgezone_filter_ver;
				str_dts = SYNA_EDGE_ZONE_FILTER_VER_ARRAY;
				break;
			default:
				pVal = NULL;
				str_dts = NULL;
				break;
		}

		if (pVal == NULL || str_dts == NULL)
			continue;

		prop = of_find_property(np, str_dts, NULL);
		if (prop && prop->length) {
			retval = of_property_read_u32_array(np, str_dts ,temp_val, SYNA_GRIP_PARAMETERS_SIZE);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent, "Failed to read synaptics, %s property\n", str_dts);
			}
		}

		/* if there is no correct setting in dts, set it to 0 */
		for (j = 0; j < SYNA_GRIP_PARAMETERS_SIZE; j++) {
			pVal[j] = temp_val[j];
		}
#ifdef GRIP_MODE_DEBUG
		LOGI(tcm_hcd->pdev->dev.parent, "%s: %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n",
								str_dts, pVal[0], pVal[1], pVal[2], pVal[3], pVal[4], pVal[5], pVal[6], pVal[7], pVal[8], pVal[9], pVal[10], pVal[11],
								pVal[12], pVal[13], pVal[14], pVal[15],	pVal[16], pVal[17], pVal[18], pVal[19], pVal[20], pVal[21], pVal[22], pVal[23],
								pVal[24], pVal[25], pVal[26], pVal[27], pVal[28], pVal[29], pVal[30], pVal[31]);
#endif
	}

	/* parse the display resolution */
	memset(temp_val, 0, sizeof(temp_val));
	str_dts = SYNA_DISPLAY_RESOLUTION_ARRAY;
	prop = of_find_property(np, str_dts, NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32_array(np, str_dts ,temp_val, SYNA_DISPLAY_RESOLUTION_SIZE);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent, "Failed to read synaptics, %s property\n", str_dts);
		}
	}
	bdata->x_max = temp_val[0];
	bdata->y_max = temp_val[1];
	LOGI(tcm_hcd->pdev->dev.parent, "%s: x_max:%2d, y_max:%2d\n",
			str_dts, bdata->x_max, bdata->y_max);

	return 0;
}

static void syna_tcm_init_touchmode_data(void)
{
	int i = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;
	struct syna_tcm_xiaomi_board_data *bdata = &tcm_hcd->xiaomi_board_data;

	if (!p_xiaomi_touch_interfaces) {
		LOGE(tcm_hcd->pdev->dev.parent,"p_xiaomi_touch_interfaces is NULL\n");
		return;
	}

	/* parse the TOUCH_MODE value in dts */
	syna_tcm_parse_gamemode_param_dt(bdata);

	/* Touch Game Mode Switch */
	p_xiaomi_touch_interfaces->touch_mode[Touch_Game_Mode][GET_MAX_VALUE] =
		bdata->game_mode[SYNA_DTS_GET_MAX_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Game_Mode][GET_MIN_VALUE] =
		bdata->game_mode[SYNA_DTS_GET_MIN_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Game_Mode][GET_DEF_VALUE] =
		bdata->game_mode[SYNA_DTS_GET_DEF_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Game_Mode][SET_CUR_VALUE] =
		bdata->game_mode[SYNA_DTS_SET_CUR_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Game_Mode][GET_CUR_VALUE] =
		bdata->game_mode[SYNA_DTS_GET_CUR_INDEX];

	/* Active Mode */
	p_xiaomi_touch_interfaces->touch_mode[Touch_Active_MODE][GET_MAX_VALUE] =
		bdata->active_mode[SYNA_DTS_GET_MAX_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Active_MODE][GET_MIN_VALUE] =
		bdata->active_mode[SYNA_DTS_GET_MIN_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Active_MODE][GET_DEF_VALUE] =
		bdata->active_mode[SYNA_DTS_GET_DEF_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Active_MODE][SET_CUR_VALUE] =
		bdata->active_mode[SYNA_DTS_SET_CUR_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Active_MODE][GET_CUR_VALUE] =
		bdata->active_mode[SYNA_DTS_GET_CUR_INDEX];

	/* Finger Hysteresis */
	p_xiaomi_touch_interfaces->touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] =
		bdata->up_threshold[SYNA_DTS_GET_MAX_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] =
		bdata->up_threshold[SYNA_DTS_GET_MIN_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] =
		bdata->up_threshold[SYNA_DTS_GET_DEF_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] =
		bdata->up_threshold[SYNA_DTS_SET_CUR_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] =
		bdata->up_threshold[SYNA_DTS_GET_CUR_INDEX];

	/* Tolerance */
	p_xiaomi_touch_interfaces->touch_mode[Touch_Tolerance][GET_MAX_VALUE] =
		bdata->tolerance[SYNA_DTS_GET_MAX_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Tolerance][GET_MIN_VALUE] =
		bdata->tolerance[SYNA_DTS_GET_MIN_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Tolerance][GET_DEF_VALUE] =
		bdata->tolerance[SYNA_DTS_GET_DEF_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Tolerance][SET_CUR_VALUE] =
		bdata->tolerance[SYNA_DTS_SET_CUR_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Tolerance][GET_CUR_VALUE] =
		bdata->tolerance[SYNA_DTS_GET_CUR_INDEX];

	/* Edge Filter */
	p_xiaomi_touch_interfaces->touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] =
		bdata->edge_filter[SYNA_DTS_GET_MAX_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] =
		bdata->edge_filter[SYNA_DTS_GET_MIN_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] =
		bdata->edge_filter[SYNA_DTS_GET_DEF_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] =
		bdata->edge_filter[SYNA_DTS_SET_CUR_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] =
		bdata->edge_filter[SYNA_DTS_GET_CUR_INDEX];

	/* Orientation */
	p_xiaomi_touch_interfaces->touch_mode[Touch_Panel_Orientation][GET_MAX_VALUE] =
		bdata->panel_orien[SYNA_DTS_GET_MAX_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Panel_Orientation][GET_MIN_VALUE] =
		bdata->panel_orien[SYNA_DTS_GET_MIN_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE] =
		bdata->panel_orien[SYNA_DTS_GET_DEF_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE] =
		bdata->panel_orien[SYNA_DTS_SET_CUR_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] =
		bdata->panel_orien[SYNA_DTS_GET_CUR_INDEX];

	/* Report Rate */
	p_xiaomi_touch_interfaces->touch_mode[Touch_Report_Rate][GET_MAX_VALUE] =
		bdata->report_rate[SYNA_DTS_GET_MAX_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Report_Rate][GET_MIN_VALUE] =
		bdata->report_rate[SYNA_DTS_GET_MIN_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Report_Rate][GET_DEF_VALUE] =
		bdata->report_rate[SYNA_DTS_GET_DEF_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Report_Rate][SET_CUR_VALUE] =
		bdata->report_rate[SYNA_DTS_SET_CUR_INDEX];
	p_xiaomi_touch_interfaces->touch_mode[Touch_Report_Rate][GET_CUR_VALUE] =
		bdata->report_rate[SYNA_DTS_GET_CUR_INDEX];

	i = 0;

#ifdef GRIP_MODE_DEBUG
	for (i = 0; i < Touch_Mode_NUM; i++) {
		LOGI(tcm_hcd->pdev->dev.parent,
			"%s, mode:%d, set cur:%d, get cur:%d, def:%d min:%d max:%d\n",
			__func__, i,
			p_xiaomi_touch_interfaces->touch_mode[i][SET_CUR_VALUE],
			p_xiaomi_touch_interfaces->touch_mode[i][GET_CUR_VALUE],
			p_xiaomi_touch_interfaces->touch_mode[i][GET_DEF_VALUE],
			p_xiaomi_touch_interfaces->touch_mode[i][GET_MIN_VALUE],
			p_xiaomi_touch_interfaces->touch_mode[i][GET_MAX_VALUE]);
	}
#endif
	return;
}
#endif

static int syna_tcm_check_f35(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char fn_number;
	int retry = 0;
	const int retry_max = 10;

F35_BOOT_RECHECK:
			retval = syna_tcm_rmi_read(tcm_hcd,
						PDT_END_ADDR,
						&fn_number,
						sizeof(fn_number));
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read F35 function number\n");
				tcm_hcd->is_detected = false;
				return -ENODEV;
			}

			LOGD(tcm_hcd->pdev->dev.parent,
					"Found F$%02x\n",
					fn_number);

			if (fn_number != RMI_UBL_FN_NUMBER) {
					LOGE(tcm_hcd->pdev->dev.parent,
							"Failed to find F$35, try_times = %d\n",
							retry);
				if (retry < retry_max) {
					msleep(100);
					retry++;
			goto F35_BOOT_RECHECK;
				}
				tcm_hcd->is_detected = false;
				return -ENODEV;
			}
	return 0;
}

static int syna_tcm_sensor_detection(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *build_id;
	unsigned int payload_length;
	unsigned int max_write_size;

	tcm_hcd->in_hdl_mode = false;
	tcm_hcd->sensor_type = TYPE_UNKNOWN;

	/* read sensor info for identification */
	retval = tcm_hcd->read_message(tcm_hcd,
			NULL,
			0);

	/* once the tcm communication interface is not ready, */
	/* check whether the device is in F35 mode        */
	if (retval < 0) {
		if (retval == -ENXIO &&
				tcm_hcd->hw_if->bus_io->type == BUS_SPI) {

			retval = syna_tcm_check_f35(tcm_hcd);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to read TCM message\n");
				return retval;
			}
			tcm_hcd->in_hdl_mode = true;
			tcm_hcd->sensor_type = TYPE_F35;
			tcm_hcd->is_detected = true;
			tcm_hcd->rd_chunk_size = HDL_RD_CHUNK_SIZE;
			tcm_hcd->wr_chunk_size = HDL_WR_CHUNK_SIZE;
			LOGN(tcm_hcd->pdev->dev.parent, "F35 mode\n");
			return retval;
		} else {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read TCM message\n");
			return retval;
		}
	}

	/* expect to get an identify report after powering on */

	if (tcm_hcd->status_report_code != REPORT_IDENTIFY) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Unexpected report code (0x%02x)\n",
				tcm_hcd->status_report_code);

		return -ENODEV;
	}

	tcm_hcd->is_detected = true;
	payload_length = tcm_hcd->payload_length;

	LOCK_BUFFER(tcm_hcd->in);

	retval = secure_memcpy((unsigned char *)&tcm_hcd->id_info,
				sizeof(tcm_hcd->id_info),
				&tcm_hcd->in.buf[MESSAGE_HEADER_SIZE],
				tcm_hcd->in.buf_size - MESSAGE_HEADER_SIZE,
				MIN(sizeof(tcm_hcd->id_info), payload_length));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy identification info\n");
		UNLOCK_BUFFER(tcm_hcd->in);
		return retval;
	}

	UNLOCK_BUFFER(tcm_hcd->in);

	build_id = tcm_hcd->id_info.build_id;
	tcm_hcd->packrat_number = le4_to_uint(build_id);

	max_write_size = le2_to_uint(tcm_hcd->id_info.max_write_size);
	tcm_hcd->wr_chunk_size = MIN(max_write_size, WR_CHUNK_SIZE);
	if (tcm_hcd->wr_chunk_size == 0)
		tcm_hcd->wr_chunk_size = max_write_size;

	if (tcm_hcd->id_info.mode == MODE_ROMBOOTLOADER) {
		tcm_hcd->in_hdl_mode = true;
		tcm_hcd->sensor_type = TYPE_ROMBOOT;
		tcm_hcd->rd_chunk_size = HDL_RD_CHUNK_SIZE;
		tcm_hcd->wr_chunk_size = HDL_WR_CHUNK_SIZE;
		LOGN(tcm_hcd->pdev->dev.parent,
					"RomBoot mode\n");
	} else if (tcm_hcd->id_info.mode == MODE_APPLICATION_FIRMWARE) {
		tcm_hcd->sensor_type = TYPE_FLASH;
		LOGN(tcm_hcd->pdev->dev.parent,
				"Application mode (build id = %d)\n",
				tcm_hcd->packrat_number);
	} else {
		LOGW(tcm_hcd->pdev->dev.parent,
				"TCM is detected, but mode is 0x%02x\n",
			tcm_hcd->id_info.mode);
	}

	return 0;
}


static ssize_t tp_irq_debug_write(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
{
	int retval = 0;
	char tmp[1];
	int ret;

	if (copy_from_user(tmp, buf, 1)) {
		pr_err("%s: copy_from_user data fail\n", __func__);
		retval = -EFAULT;
	}
	ret = (int)&tmp;
	pr_err("%s: ret = %d\n", __func__, ret);
	if(ret)
		disable_irq(gloab_tcm_hcd->irq);
	else
		enable_irq(gloab_tcm_hcd->irq);

	return retval;

}

static const struct file_operations tp_irq_debug_ops = {
	.write = tp_irq_debug_write,
};

#ifdef SYNAPTICS_DEBUGFS_ENABLE
static void syna_tcm_dbg_suspend(struct syna_tcm_hcd *tcm_hcd, bool enable)
{
	if (enable) {
		queue_work(tcm_hcd->event_wq, &tcm_hcd->early_suspend_work);
		queue_work(tcm_hcd->event_wq, &tcm_hcd->suspend_work);
	} else
		queue_work(tcm_hcd->event_wq, &tcm_hcd->resume_work);
}

static int syna_tcm_dbg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t syna_tcm_dbg_read(struct file *file, char __user *buf, size_t size,
				loff_t *ppos)
{
	const char *str = "cmd support as below:\n \
	\necho \"irq-disable\" or \"irq-enable\" to ctrl irq\n \
	\necho \"tp-suspend-en\" or \"tp-suspend-off\" to ctrl panel in or off suspend status\n \
	\necho \"tp-sd-en\" or \"tp-sd-off\" to ctrl panel in or off sleep status\n";

	loff_t pos = *ppos;
	int len = strlen(str);

	if (pos < 0)
		return -EINVAL;
	if (pos >= len)
		return 0;

	if (copy_to_user(buf, str, len))
		return -EFAULT;

	*ppos = pos + len;

	return len;
}

static ssize_t syna_tcm_dbg_write(struct file *file, const char __user *buf,
					size_t size, loff_t *ppos)
{
	char *cmd = kzalloc(size + 1, GFP_KERNEL);
	int ret = size;

	if (!cmd)
		return -ENOMEM;

	if (copy_from_user(cmd, buf, size)) {
		ret = -EFAULT;
		goto out;
	}

	cmd[size] = '\0';

	if (!strncmp(cmd, "irq-disable", 11)) {
		LOGI(gloab_tcm_hcd->pdev->dev.parent,
			"%s touch irq is disabled!\n", __func__);
		gloab_tcm_hcd->enable_irq(gloab_tcm_hcd, false, true);
	} else if (!strncmp(cmd, "irq-enable", 10)) {
		LOGI(gloab_tcm_hcd->pdev->dev.parent,
			"%s touch irq is enabled!\n", __func__);
		gloab_tcm_hcd->enable_irq(gloab_tcm_hcd, true, NULL);
	} else if (!strncmp(cmd, "tp-sd-en", 8))
		syna_tcm_dbg_suspend(gloab_tcm_hcd, true);
	else if (!strncmp(cmd, "tp-sd-off", 9))
		syna_tcm_dbg_suspend(gloab_tcm_hcd, false);
	else if (!strncmp(cmd, "tp-suspend-en", 13))
		syna_tcm_dbg_suspend(gloab_tcm_hcd, true);
	else if (!strncmp(cmd, "tp-suspend-off", 14))
		syna_tcm_dbg_suspend(gloab_tcm_hcd, false);
out:
	kfree(cmd);

	return ret;
}

static int syna_tcm_dbg_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

static void syna_tcm_debugfs_exit(void)
{
	debugfs_remove_recursive(gloab_tcm_hcd->debugfs);
}


static const struct file_operations tpdbg_operations = {
	.owner = THIS_MODULE,
	.open = syna_tcm_dbg_open,
	.read = syna_tcm_dbg_read,
	.write = syna_tcm_dbg_write,
	.release = syna_tcm_dbg_release,
};
#endif

#ifdef SYNAPTICS_POWERSUPPLY_CB
static int syna_tcm_get_charging_status()
{
#ifdef CONFIG_QGKI_SYSTEM
	int is_charging = 0;
	is_charging = !!power_supply_is_system_supplied();
	if (!is_charging)
		return NOT_CHARGING;
	else
		return CHARGING;
#else
	return NOT_CHARGING;
#endif
}

static void syna_tcm_power_supply_work(struct work_struct *work)
{
	int charging_status;

	LOGI(gloab_tcm_hcd->pdev->dev.parent, "%s enter!\n", __func__);
	if (!gloab_tcm_hcd || !tp_probe_success) {
		LOGE(gloab_tcm_hcd->pdev->dev.parent,
			"%s touch is not inited\n", __func__);
		return;
	}

	charging_status = syna_tcm_get_charging_status();

	if (charging_status != gloab_tcm_hcd->charging_status || gloab_tcm_hcd->charging_status < 0) {
		gloab_tcm_hcd->charging_status = charging_status;
		gloab_tcm_hcd->charger_connected = (charging_status == CHARGING) ? 1 : 0;
		if (gloab_tcm_hcd->in_suspend) {
			LOGI(gloab_tcm_hcd->pdev->dev.parent,
				"%s Can't write charge status\n", __func__);
			return;
		}
		syna_tcm_set_charge_status();
	}
}

static int syna_tcm_power_supply_event(struct notifier_block *nb, unsigned long event, void *ptr)
{
	if (!gloab_tcm_hcd)
		return 0;

	schedule_delayed_work(&gloab_tcm_hcd->power_supply_work, msecs_to_jiffies(500));
	return 0;
}
#endif


#ifdef SYNA_TCM_XIAOMI_TOUCHFEATURE
static ssize_t syna_tcm_fw_version_read(struct file *file, char __user *buf,
				   size_t count, loff_t *pos)
{
	int cnt = 0, ret = 0;
	char tmp[TP_INFO_MAX_LENGTH];
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	if (*pos != 0)
		return 0;

	if (!tcm_hcd)
		return 0;

	if (tcm_hcd->in_suspend)
		return 0;

	cnt =
	    snprintf(tmp, TP_INFO_MAX_LENGTH, "Firmware: %d Cfg: %02x %02x\n",
			tcm_hcd->packrat_number,
			tcm_hcd->app_info.customer_config_id[6] - 48,
			tcm_hcd->app_info.customer_config_id[7] - 48);
	ret = copy_to_user(buf, tmp, cnt);

	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}

static const struct file_operations syna_tcm_fw_version_ops = {
	.read = syna_tcm_fw_version_read,
};

static ssize_t syna_tcm_lockdown_info_read(struct file *file, char __user *buf,
				      size_t count, loff_t *pos)
{
	int cnt = 0, ret = 0;
	char tmp[TP_INFO_MAX_LENGTH];
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	if (!tcm_hcd)
		return 0;

	if (tcm_hcd->in_suspend)
		return 0;

	if (*pos != 0)
		return 0;

	memset(tmp, 0, TP_INFO_MAX_LENGTH);

	if (!tcm_hcd->syna_tcm_lockdown_info)
		return 0;

	ret = tcm_hcd->syna_tcm_lockdown_info();
	if (ret) {
		LOGE(tcm_hcd->pdev->dev.parent, "get lockdown info error\n");
		goto out;
	}

	cnt =
		snprintf(tmp, PAGE_SIZE,
			 "OEM_INFO: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
			 tcm_hcd->lockdown_info[0], tcm_hcd->lockdown_info[1],
			 tcm_hcd->lockdown_info[2], tcm_hcd->lockdown_info[3],
			 tcm_hcd->lockdown_info[4], tcm_hcd->lockdown_info[5],
			 tcm_hcd->lockdown_info[6], tcm_hcd->lockdown_info[7]);
	ret = copy_to_user(buf, tmp, cnt);

out:
	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}

static const struct file_operations syna_tcm_lockdown_info_ops = {
	.read = syna_tcm_lockdown_info_read,
};

static int sum_size;
static int left_size;
static int transfer_size;
static char *tmp;
static ssize_t syna_tcm_datadump_read(struct file *file, char __user *buf,
				 size_t count, loff_t *pos)
{
	int ret = 0, cnt1 = 0, cnt2 = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	if (!tcm_hcd)
		return 0;

	if (tcm_hcd->in_suspend)
		return 0;

	LOGD(tcm_hcd->pdev->dev.parent, "Before: transfer_size = %d, left_size = %d, pos = %d\n", transfer_size, left_size, *pos);

	if (*pos == 0) {
		tmp = vmalloc(PAGE_SIZE * 2);
		if (tmp == NULL)
			return 0;
		else
			memset(tmp, 0, PAGE_SIZE * 2);

		if (!tcm_hcd->testing_xiaomi_report_data) {
			LOGE(tcm_hcd->pdev->dev.parent, "tcm_hcd->testing_xiaomi_report_data = NULL\n");
			ret = -EINVAL;
			goto err_out;
		}

		cnt1 = tcm_hcd->testing_xiaomi_report_data(REPORT_DELTA, tmp); /* REPORT_DELTA = 0x12 */
		LOGD(tcm_hcd->pdev->dev.parent, "cnt1 = %d\n", cnt1);
		if (cnt1 <= 0) {
			LOGE(tcm_hcd->pdev->dev.parent, "REPORT_DELTA failed\n");
			ret = cnt1;
			goto err_out;
		}

		cnt2 = tcm_hcd->testing_xiaomi_report_data(REPORT_RID161, tmp + cnt1); /* REPORT_RID161 = 0xA1 */
		LOGD(tcm_hcd->pdev->dev.parent, "cnt2 = %d\n", cnt2);
		if (cnt2 <= 0) {
			LOGE(tcm_hcd->pdev->dev.parent, "REPORT_RID161 failed\n");
			ret = cnt2;
			goto err_out;
		}
		sum_size = cnt1 + cnt2;
		left_size = sum_size;
	} else {
		if (tmp && (*pos >= sum_size)) {
			vfree(tmp);
			tmp = NULL;
			return 0;
		}
	}

	if (left_size < PAGE_SIZE)
		transfer_size = left_size;
	else {
		transfer_size = PAGE_SIZE;
	}
	left_size -= transfer_size;

	LOGD(tcm_hcd->pdev->dev.parent, "1: sum_size = %d, left_size = %d, transfer_size = %d\n", sum_size, left_size, transfer_size);

	if (access_ok(buf, sum_size)) {
		ret = copy_to_user(buf, (tmp + *pos), transfer_size);
		if (ret != 0) {
			LOGE(tcm_hcd->pdev->dev.parent, "copy to user failed, ret = %d\n", ret);
			ret = -EFAULT;
			goto err_out;
		} else {
			LOGD(tcm_hcd->pdev->dev.parent, "copy to user success, ret = %d\n", ret);
		}
	}

	*pos += transfer_size;
	LOGD(tcm_hcd->pdev->dev.parent, "After: transfer_size = %d, left_size = %d, pos = %d\n", transfer_size, left_size, *pos);
	return transfer_size;
err_out:
	if (tmp) {
		vfree(tmp);
		tmp = NULL;
	}
	return ret;
}

static const struct file_operations syna_tcm_datadump_ops = {
	.read = syna_tcm_datadump_read,
};

static ssize_t syna_tcm_selftest_read(struct file *file, char __user *buf,
				 size_t count, loff_t *pos)
{
	char tmp[5] = { 0 };
	int cnt;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	if (!tcm_hcd)
		return 0;

	if (*pos != 0)
		return 0;

	cnt =
	    snprintf(tmp, sizeof(tcm_hcd->maintenance_result), "%d\n",
		     tcm_hcd->maintenance_result);
	if (copy_to_user(buf, tmp, strlen(tmp))) {
		return -EFAULT;
	}
	*pos += cnt;
	return cnt;
}

static ssize_t syna_tcm_selftest_write(struct file *file, const char __user *buf,
				  size_t count, loff_t *pos)
{
	int retval = 0;
	char *self_test_data = NULL;
	char tmp[6];
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	if (!tcm_hcd)
		return 0;

	if (copy_from_user(tmp, buf, count)) {
		retval = -EFAULT;
		goto out;
	}

	if (!strncmp("short", tmp, 5) || !strncmp("open", tmp, 4)) {
		if (!tcm_hcd && !tcm_hcd->testing_xiaomi_self_test) {
			LOGE(tcm_hcd->pdev->dev.parent, "NULL Pointer!\n");
			retval = 1;
			goto out;
		}

		self_test_data = vmalloc(PAGE_SIZE * 3);
		if (self_test_data == NULL) {
			retval = 1;
			goto out;
		}

		retval = tcm_hcd->testing_xiaomi_self_test(self_test_data);
		if (!retval) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"self test failed, retval = %d\n", retval);
			retval = 1;
			goto out;
		}
		retval = 2;
	} else if (!strncmp("i2c", tmp, 3)) {
		if (!tcm_hcd->testing_xiaomi_chip_id_read) {
			retval = 1;
			goto out;
		}

		retval = tcm_hcd->testing_xiaomi_chip_id_read();
		/* (retval == 2) passed  (retval == 1) failed */
	}
out:
	tcm_hcd->maintenance_result = retval;

	if (self_test_data) {
		vfree(self_test_data);
		self_test_data = NULL;
	}

	if (retval >= 0)
		retval = count;

	return retval;
}

static const struct file_operations syna_tcm_selftest_ops = {
	.read = syna_tcm_selftest_read,
	.write = syna_tcm_selftest_write,
};

static u8 syna_tcm_panel_vendor_read(void)
{
	int ret = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	if (!tcm_hcd)
		return 0;

	if (tcm_hcd->in_suspend)
		return 0;

	if (tp_probe_success &&
			tcm_hcd->syna_tcm_lockdown_info != NULL) {
		ret = tcm_hcd->syna_tcm_lockdown_info();
		if (ret) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"%s: get lockdown info error\n", __func__);
			return 0;
		}
		return tcm_hcd->lockdown_info[0];
	}

	return 0;
}

static u8 syna_tcm_panel_color_read(void)
{
	int ret = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	if (!tcm_hcd)
		return 0;

	if (tcm_hcd->in_suspend)
		return 0;

	if (tp_probe_success &&
			tcm_hcd->syna_tcm_lockdown_info != NULL) {
		ret = tcm_hcd->syna_tcm_lockdown_info();
		if (ret) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"%s: get lockdown info error\n", __func__);
			return 0;
		}
		return tcm_hcd->lockdown_info[2];
	}

	return 0;
}

static u8 syna_tcm_panel_display_read(void)
{
	int ret = 0;
	struct syna_tcm_hcd *tcm_hcd = gloab_tcm_hcd;

	if (!tcm_hcd)
		return 0;

	if (tcm_hcd->in_suspend)
		return 0;

	if (tp_probe_success &&
			tcm_hcd->syna_tcm_lockdown_info != NULL) {
		ret = tcm_hcd->syna_tcm_lockdown_info();
		if (ret) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"%s: get lockdown info error\n", __func__);
			return 0;
		}
		return tcm_hcd->lockdown_info[1];
	}

	return 0;
}

static char syna_tcm_touch_vendor_read(void)
{
	return '5';
}
#endif

static int syna_tcm_probe(struct platform_device *pdev)
{
	int retval;
	int idx;
	struct syna_tcm_hcd *tcm_hcd;
	const struct syna_tcm_board_data *bdata;
	const struct syna_tcm_hw_interface *hw_if;
	struct spi_device *spi;

	LOGI(&pdev->dev, "-----enter-----%s\n", __func__);
	hw_if = pdev->dev.platform_data;
	if (!hw_if) {
		LOGE(&pdev->dev,
				"Hardware interface not found\n");
		return -ENODEV;
	}

	bdata = hw_if->bdata;
	if (!bdata) {
		LOGE(&pdev->dev,
				"Board data not found\n");
		return -ENODEV;
	}

	tcm_hcd = kzalloc(sizeof(*tcm_hcd), GFP_KERNEL);
	if (!tcm_hcd) {
		LOGE(&pdev->dev,
				"Failed to allocate memory for tcm_hcd\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, tcm_hcd);
	gloab_tcm_hcd = tcm_hcd;

	tcm_hcd->pdev = pdev;
	tcm_hcd->hw_if = hw_if;
	tcm_hcd->reset = syna_tcm_reset;
	tcm_hcd->reset_n_reinit = syna_tcm_reset_and_reinit;
	tcm_hcd->sleep = syna_tcm_sleep;
	tcm_hcd->identify = syna_tcm_identify;
	tcm_hcd->enable_irq = syna_tcm_enable_irq;
	tcm_hcd->switch_mode = syna_tcm_switch_mode;
	tcm_hcd->read_message = syna_tcm_read_message;
	tcm_hcd->write_message = syna_tcm_write_message;
	tcm_hcd->get_dynamic_config = syna_tcm_get_dynamic_config;
	tcm_hcd->set_dynamic_config = syna_tcm_set_dynamic_config;
	tcm_hcd->get_data_location = syna_tcm_get_data_location;

	tcm_hcd->rd_chunk_size = RD_CHUNK_SIZE;
	tcm_hcd->wr_chunk_size = WR_CHUNK_SIZE;
	tcm_hcd->is_detected = false;
	tcm_hcd->wakeup_gesture_enabled = WAKEUP_GESTURE;
	tcm_hcd->fod_enabled = false;
	tcm_hcd->in_suspending = false;
	tcm_hcd->lockdown_info_ready = false;

#ifdef PREDICTIVE_READING
	tcm_hcd->read_length = MIN_READ_LENGTH;
#else
	tcm_hcd->read_length = MESSAGE_HEADER_SIZE;
#endif

#ifdef WATCHDOG_SW
	tcm_hcd->watchdog.run = RUN_WATCHDOG;
	tcm_hcd->update_watchdog = syna_tcm_update_watchdog;
#endif

	if (bdata->irq_gpio >= 0)
		tcm_hcd->irq = gpio_to_irq(bdata->irq_gpio);
	else
		tcm_hcd->irq = bdata->irq_gpio;

	mutex_init(&tcm_hcd->extif_mutex);
	mutex_init(&tcm_hcd->reset_mutex);
	mutex_init(&tcm_hcd->irq_en_mutex);
	mutex_init(&tcm_hcd->io_ctrl_mutex);
	mutex_init(&tcm_hcd->rw_ctrl_mutex);
	mutex_init(&tcm_hcd->command_mutex);
	mutex_init(&tcm_hcd->identify_mutex);
	mutex_init(&tcm_hcd->esd_recovery_mutex);

	INIT_BUFFER(tcm_hcd->in, false);
	INIT_BUFFER(tcm_hcd->out, false);
	INIT_BUFFER(tcm_hcd->resp, true);
	INIT_BUFFER(tcm_hcd->temp, false);
	INIT_BUFFER(tcm_hcd->config, false);
	INIT_BUFFER(tcm_hcd->report.buffer, true);

	LOCK_BUFFER(tcm_hcd->in);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&tcm_hcd->in,
			tcm_hcd->read_length + 1);
	if (retval < 0) {
		LOGE(&pdev->dev,
				"Failed to allocate memory for tcm_hcd->in.buf\n");
		UNLOCK_BUFFER(tcm_hcd->in);
		goto err_alloc_mem;
	}

	UNLOCK_BUFFER(tcm_hcd->in);

	atomic_set(&tcm_hcd->command_status, CMD_IDLE);

	atomic_set(&tcm_hcd->helper.task, HELP_NONE);

	device_init_wakeup(&pdev->dev, 1);

	init_waitqueue_head(&tcm_hcd->hdl_wq);

	init_waitqueue_head(&tcm_hcd->reflash_wq);
	atomic_set(&tcm_hcd->firmware_flashing, 0);

	if (!mod_pool.initialized) {
		mutex_init(&mod_pool.mutex);
		INIT_LIST_HEAD(&mod_pool.list);
		mod_pool.initialized = true;
	}

	spi = to_spi_device(tcm_hcd->pdev->dev.parent);
	tcm_hcd->pinctrl = devm_pinctrl_get(&spi->dev);
	if (IS_ERR(tcm_hcd->pinctrl)) {
		LOGE(&pdev->dev, "Cannot find default pinctrl, ret = %d!\n",
			PTR_ERR(tcm_hcd->pinctrl));
	} else {
		tcm_hcd->pins_default =
			pinctrl_lookup_state(tcm_hcd->pinctrl, "default");
		if (IS_ERR(tcm_hcd->pins_default))
			LOGE(&pdev->dev,"Cannot find pinctrl default %d!\n",
				PTR_ERR(tcm_hcd->pins_default));
		else
			pinctrl_select_state(tcm_hcd->pinctrl,
				tcm_hcd->pins_default);
	}

	retval = syna_tcm_regulator_init(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get regulators\n");
		goto err_regulator_init;
	}

	retval = syna_tcm_enable_regulator(tcm_hcd, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to enable regulators\n");
		goto err_enable_regulator;
	}

	retval = syna_tcm_config_gpio(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to configure GPIO's\n");
		goto err_config_gpio;
	}

	/* detect the type of touch controller */
	retval = syna_tcm_sensor_detection(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to detect the sensor\n");
		goto err_sysfs_create_dir;
	}

	sysfs_dir = kobject_create_and_add(PLATFORM_DRIVER_NAME,
			&pdev->dev.kobj);
	if (!sysfs_dir) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs directory\n");
		retval = -EINVAL;
		goto err_sysfs_create_dir;
	}

	tcm_hcd->sysfs_dir = sysfs_dir;

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
		retval = sysfs_create_file(tcm_hcd->sysfs_dir,
				&(*attrs[idx]).attr);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to create sysfs file\n");
			goto err_sysfs_create_file;
		}
	}

	tcm_hcd->dynamnic_config_sysfs_dir =
			kobject_create_and_add(DYNAMIC_CONFIG_SYSFS_DIR_NAME,
			tcm_hcd->sysfs_dir);
	if (!tcm_hcd->dynamnic_config_sysfs_dir) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create dynamic config sysfs directory\n");
		retval = -EINVAL;
		goto err_sysfs_create_dynamic_config_dir;
	}

	for (idx = 0; idx < ARRAY_SIZE(dynamic_config_attrs); idx++) {
		retval = sysfs_create_file(tcm_hcd->dynamnic_config_sysfs_dir,
				&(*dynamic_config_attrs[idx]).attr);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to create dynamic config sysfs file\n");
			goto err_sysfs_create_dynamic_config_file;
		}
	}

	tcm_hcd->tp_irq_debug =
		proc_create("tp_irq_debug", 0664, NULL, &tp_irq_debug_ops);

#ifdef REPORT_NOTIFIER
	tcm_hcd->notifier_thread = kthread_run(syna_tcm_report_notifier,
			tcm_hcd, "syna_tcm_report_notifier");
	if (IS_ERR(tcm_hcd->notifier_thread)) {
		retval = PTR_ERR(tcm_hcd->notifier_thread);
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create and run tcm_hcd->notifier_thread\n");
		goto err_create_run_kthread;
	}
#endif

	tcm_hcd->helper.workqueue =
			create_singlethread_workqueue("syna_tcm_helper");
	INIT_WORK(&tcm_hcd->helper.work, syna_tcm_helper_work);

#ifdef WATCHDOG_SW
	tcm_hcd->watchdog.workqueue =
			create_singlethread_workqueue("syna_tcm_watchdog");
	INIT_DELAYED_WORK(&tcm_hcd->watchdog.work, syna_tcm_watchdog_work);
#endif

	tcm_hcd->polling_workqueue =
			create_singlethread_workqueue("syna_tcm_polling");
	INIT_DELAYED_WORK(&tcm_hcd->polling_work, syna_tcm_polling_work);

	tcm_hcd->event_wq = alloc_workqueue("syna_tcm_event_queue",
					WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!tcm_hcd->event_wq) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create event_wq\n");
		retval = -ENOMEM;
		goto err_pm_event_wq;
	}

	INIT_WORK(&tcm_hcd->resume_work, syna_tcm_resume_work);
	INIT_WORK(&tcm_hcd->early_suspend_work, syna_tcm_early_suspend_work);
	INIT_WORK(&tcm_hcd->suspend_work, syna_tcm_suspend_work);
	INIT_WORK(&tcm_hcd->fod_work, syna_tcm_fod_work);
	INIT_WORK(&tcm_hcd->set_report_rate_work, syna_tcm_set_report_rate_work);

	tcm_hcd->fb_notifier = syna_tcm_noti_block;
	if (mi_disp_register_client(&tcm_hcd->fb_notifier) < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"ERROR: register notifier failed!\n");
	}

#ifdef SYNA_TCM_XIAOMI_TOUCHFEATURE
	tcm_hcd->game_wq = alloc_workqueue("syna_tcm_game_queue",
					WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!tcm_hcd->game_wq) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create game_wq\n");
		retval = -ENOMEM;
		goto err_xiaomi_touchfeature;
	}

	INIT_WORK(&tcm_hcd->cmd_update_work, syna_tcm_cmd_update_work);
	INIT_WORK(&tcm_hcd->grip_mode_work, syna_tcm_grip_mode_work);
	mutex_init(&tcm_hcd->cmd_update_mutex);

	if (!p_xiaomi_touch_interfaces) {
		p_xiaomi_touch_interfaces =
			kmalloc(sizeof(struct xiaomi_touch_interface), GFP_KERNEL);
		if (!p_xiaomi_touch_interfaces) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for p_xiaomi_touch_interfaces\n");
			retval = -ENOMEM;
			goto err_xiaomi_touchfeature;
		}
	}

	mutex_init(&tcm_hcd->long_mode_value_mutex);
	memset(p_xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
	p_xiaomi_touch_interfaces->setModeValue = syna_tcm_set_cur_value;
	p_xiaomi_touch_interfaces->setModeLongValue = syna_tcm_set_mode_long_value;
	p_xiaomi_touch_interfaces->getModeValue = syna_tcm_get_mode_value;
	p_xiaomi_touch_interfaces->getModeAll = syna_tcm_get_mode_all;
	p_xiaomi_touch_interfaces->resetMode = syna_tcm_reset_mode;
	p_xiaomi_touch_interfaces->palm_sensor_write = syna_tcm_palm_sensor_write;
	p_xiaomi_touch_interfaces->get_touch_rx_num = NULL;
	p_xiaomi_touch_interfaces->get_touch_tx_num = NULL;
	p_xiaomi_touch_interfaces->get_touch_x_resolution = NULL;
	p_xiaomi_touch_interfaces->get_touch_y_resolution = NULL;
	p_xiaomi_touch_interfaces->enable_touch_raw = NULL;
	p_xiaomi_touch_interfaces->enable_clicktouch_raw = NULL;
	p_xiaomi_touch_interfaces->enable_touch_delta = NULL;
	p_xiaomi_touch_interfaces->panel_vendor_read = syna_tcm_panel_vendor_read;
	p_xiaomi_touch_interfaces->panel_color_read = syna_tcm_panel_color_read;
	p_xiaomi_touch_interfaces->panel_display_read = syna_tcm_panel_display_read;
	p_xiaomi_touch_interfaces->touch_vendor_read = syna_tcm_touch_vendor_read;
	tcm_hcd->syna_tcm_class = get_xiaomi_touch_class();
#else
	tcm_hcd->syna_tcm_class = class_create(THIS_MODULE, "touch");
#endif

	tcm_hcd->syna_tcm_dev =
		device_create(tcm_hcd->syna_tcm_class, NULL,
			tcm_hcd->tp_dev_num, tcm_hcd, "tp_dev");
	if (IS_ERR(tcm_hcd->syna_tcm_dev)) {
		LOGE(tcm_hcd->pdev->dev.parent,
			 "Failed to create device for the sysfs!\n");
	}
	dev_set_drvdata(tcm_hcd->syna_tcm_dev, tcm_hcd);

	retval = sysfs_create_file(&tcm_hcd->syna_tcm_dev->kobj,
				&dev_attr_fod_test.attr);
	if (retval) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to create fod_test sysfs group!\n");
	}

#ifdef SYNAPTICS_POWERSUPPLY_CB
	INIT_DELAYED_WORK(&tcm_hcd->power_supply_work, syna_tcm_power_supply_work);
	tcm_hcd->charging_status = -1;
	tcm_hcd->power_supply_notifier.notifier_call = syna_tcm_power_supply_event;
	retval = power_supply_reg_notifier(&tcm_hcd->power_supply_notifier);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"usb online notifier registration error. err:%d\n", retval);
	}
#endif

#ifdef SYNAPTICS_DEBUGFS_ENABLE
	tcm_hcd->debugfs = debugfs_create_dir("tp_debug", NULL);
	if (tcm_hcd->debugfs) {
		debugfs_create_file("switch_state", 0660, tcm_hcd->debugfs, tcm_hcd,
					&tpdbg_operations);
	}
#endif

	init_completion(&tcm_hcd->pm_resume_completion);
	tcm_hcd->tp_lockdown_info_proc =
	    proc_create("tp_lockdown_info", 0444, NULL, &syna_tcm_lockdown_info_ops);
	tcm_hcd->tp_fw_version_proc =
	    proc_create("tp_fw_version", 0444, NULL, &syna_tcm_fw_version_ops);
	tcm_hcd->tp_data_dump_proc =
	    proc_create("tp_data_dump", 0444, NULL, &syna_tcm_datadump_ops);
	tcm_hcd->tp_selftest_proc =
	    proc_create("tp_selftest", 0644, NULL, &syna_tcm_selftest_ops);

	/* since the fw is not ready for hdl devices */
	if (tcm_hcd->in_hdl_mode)
		goto prepare_modules;


	/* register and enable the interrupt in probe */
	/* if this is not the hdl device */
	retval = tcm_hcd->enable_irq(tcm_hcd, true, NULL);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to enable interrupt\n");
		goto err_enable_irq;
	}
	LOGD(tcm_hcd->pdev->dev.parent,
			"Interrupt is registered\n");

	/* ensure the app firmware is running */
	retval = syna_tcm_identify(tcm_hcd, false);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Application firmware is not running\n");
		goto err_enable_irq;
	}
	/* initialize the touch reporting */
	retval = touch_init(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to initialze touch reporting\n");
		goto err_enable_irq;
	}

#ifdef SYNA_TCM_XIAOMI_TOUCHFEATURE
	if (syna_tcm_read_touchmode_data()) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"read touchmode data from IC failed!\n");
	}
	syna_tcm_init_touchmode_data();
	xiaomitouch_register_modedata(0, p_xiaomi_touch_interfaces);
#endif

prepare_modules:

	/* prepare to add other modules */
	mod_pool.workqueue =
			create_singlethread_workqueue("syna_tcm_module");
	INIT_WORK(&mod_pool.work, syna_tcm_module_work);
	mod_pool.tcm_hcd = tcm_hcd;
	mod_pool.queue_work = true;
	queue_work(mod_pool.workqueue, &mod_pool.work);

	tp_probe_success = true;
	return 0;

err_enable_irq:
	cancel_delayed_work_sync(&tcm_hcd->power_supply_work);
	power_supply_unreg_notifier(&tcm_hcd->power_supply_notifier);
	cancel_delayed_work_sync(&tcm_hcd->polling_work);
	flush_workqueue(tcm_hcd->polling_workqueue);
	destroy_workqueue(tcm_hcd->polling_workqueue);

#ifdef WATCHDOG_SW
	cancel_delayed_work_sync(&tcm_hcd->watchdog.work);
	flush_workqueue(tcm_hcd->watchdog.workqueue);
	destroy_workqueue(tcm_hcd->watchdog.workqueue);
#endif

	cancel_work_sync(&tcm_hcd->helper.work);
	flush_workqueue(tcm_hcd->helper.workqueue);
	destroy_workqueue(tcm_hcd->helper.workqueue);

#ifdef SYNA_TCM_XIAOMI_TOUCHFEATURE
err_xiaomi_touchfeature:
	syna_tcm_xiaomi_touchfeature_exit(tcm_hcd);
#endif

	if (tcm_hcd->tp_lockdown_info_proc)
		remove_proc_entry("tp_lockdown_info", NULL);
	if (tcm_hcd->tp_fw_version_proc)
		remove_proc_entry("tp_fw_version", NULL);
	if (tcm_hcd->tp_data_dump_proc)
		remove_proc_entry("tp_data_dump", NULL);
	if (tcm_hcd->tp_selftest_proc)
		remove_proc_entry("tp_selftest", NULL);
	tcm_hcd->tp_lockdown_info_proc = NULL;
	tcm_hcd->tp_fw_version_proc = NULL;
	tcm_hcd->tp_data_dump_proc = NULL;
	tcm_hcd->tp_selftest_proc = NULL;

#ifdef SYNAPTICS_DEBUGFS_ENABLE
	syna_tcm_debugfs_exit();
#endif

err_pm_event_wq:
	/* cancel_work_sync(&tcm_hcd->early_suspend_work);
	cancel_work_sync(&tcm_hcd->suspend_work);
	cancel_work_sync(&tcm_hcd->resume_work);
	flush_workqueue(tcm_hcd->event_wq); */
	destroy_workqueue(tcm_hcd->event_wq);

#ifdef REPORT_NOTIFIER
	kthread_stop(tcm_hcd->notifier_thread);
err_create_run_kthread:
#endif

	mi_disp_unregister_client(&tcm_hcd->fb_notifier);

err_sysfs_create_dynamic_config_file:
	for (idx--; idx >= 0; idx--) {
		sysfs_remove_file(tcm_hcd->dynamnic_config_sysfs_dir,
				&(*dynamic_config_attrs[idx]).attr);
	}

	kobject_put(tcm_hcd->dynamnic_config_sysfs_dir);

	idx = ARRAY_SIZE(attrs);

err_sysfs_create_dynamic_config_dir:
err_sysfs_create_file:
	for (idx--; idx >= 0; idx--)
		sysfs_remove_file(tcm_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(tcm_hcd->sysfs_dir);

err_sysfs_create_dir:
	if (bdata->irq_gpio >= 0)
		syna_tcm_set_gpio(tcm_hcd, bdata->irq_gpio, false, 0, 0);

	if (bdata->power_gpio >= 0)
		syna_tcm_set_gpio(tcm_hcd, bdata->power_gpio, false, 0, 0);

	if (bdata->reset_gpio >= 0)
		syna_tcm_set_gpio(tcm_hcd, bdata->reset_gpio, false, 0, 0);

	syna_tcm_enable_regulator(tcm_hcd, false);

err_config_gpio:
err_enable_regulator:
	devm_regulator_put(tcm_hcd->avdd);
	devm_regulator_put(tcm_hcd->iovdd);
	tcm_hcd->avdd = NULL;
	tcm_hcd->iovdd = NULL;

err_regulator_init:
	device_init_wakeup(&pdev->dev, 0);

err_alloc_mem:
	RELEASE_BUFFER(tcm_hcd->report.buffer);
	RELEASE_BUFFER(tcm_hcd->config);
	RELEASE_BUFFER(tcm_hcd->temp);
	RELEASE_BUFFER(tcm_hcd->resp);
	RELEASE_BUFFER(tcm_hcd->out);
	RELEASE_BUFFER(tcm_hcd->in);
	kfree(tcm_hcd);
	tcm_hcd = NULL;
	gloab_tcm_hcd = NULL;

	return retval;
}

static int syna_tcm_remove(struct platform_device *pdev)
{
	int idx;
	struct syna_tcm_module_handler *mod_handler;
	struct syna_tcm_module_handler *tmp_handler;
	struct syna_tcm_hcd *tcm_hcd = platform_get_drvdata(pdev);
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	LOGI(tcm_hcd->pdev->dev.parent, "syna_tcm_remove enter!\n");
	tp_probe_success = false;
	touch_remove(tcm_hcd);

	mutex_lock(&mod_pool.mutex);

	if (!list_empty(&mod_pool.list)) {
		list_for_each_entry_safe(mod_handler,
				tmp_handler,
				&mod_pool.list,
				link) {
			if (mod_handler->mod_cb->remove)
				mod_handler->mod_cb->remove(tcm_hcd);
			list_del(&mod_handler->link);
			kfree(mod_handler);
		}
	}

	mod_pool.queue_work = false;
	cancel_work_sync(&mod_pool.work);
	flush_workqueue(mod_pool.workqueue);
	destroy_workqueue(mod_pool.workqueue);

	mutex_unlock(&mod_pool.mutex);

	cancel_delayed_work_sync(&tcm_hcd->power_supply_work);
	power_supply_unreg_notifier(&tcm_hcd->power_supply_notifier);

	if (tcm_hcd->irq_enabled && bdata->irq_gpio >= 0) {
		disable_irq(tcm_hcd->irq);
		free_irq(tcm_hcd->irq, tcm_hcd);
	}

	cancel_delayed_work_sync(&tcm_hcd->polling_work);
	flush_workqueue(tcm_hcd->polling_workqueue);
	destroy_workqueue(tcm_hcd->polling_workqueue);

#ifdef WATCHDOG_SW
	cancel_delayed_work_sync(&tcm_hcd->watchdog.work);
	flush_workqueue(tcm_hcd->watchdog.workqueue);
	destroy_workqueue(tcm_hcd->watchdog.workqueue);
#endif

	cancel_work_sync(&tcm_hcd->helper.work);
	flush_workqueue(tcm_hcd->helper.workqueue);
	destroy_workqueue(tcm_hcd->helper.workqueue);

	cancel_work_sync(&tcm_hcd->early_suspend_work);
	cancel_work_sync(&tcm_hcd->suspend_work);
	cancel_work_sync(&tcm_hcd->resume_work);
	cancel_work_sync(&tcm_hcd->set_report_rate_work);
	flush_workqueue(tcm_hcd->event_wq);
	destroy_workqueue(tcm_hcd->event_wq);

#ifdef SYNA_TCM_XIAOMI_TOUCHFEATURE
	syna_tcm_xiaomi_touchfeature_exit(tcm_hcd);
#endif

#ifdef REPORT_NOTIFIER
	kthread_stop(tcm_hcd->notifier_thread);
#endif

	mi_disp_unregister_client(&tcm_hcd->fb_notifier);

	if (tcm_hcd->tp_lockdown_info_proc)
		remove_proc_entry("tp_lockdown_info", NULL);
	if (tcm_hcd->tp_fw_version_proc)
		remove_proc_entry("tp_fw_version", NULL);
	if (tcm_hcd->tp_data_dump_proc)
		remove_proc_entry("tp_data_dump", NULL);
	if (tcm_hcd->tp_selftest_proc)
		remove_proc_entry("tp_selftest", NULL);
	tcm_hcd->tp_lockdown_info_proc = NULL;
	tcm_hcd->tp_fw_version_proc = NULL;
	tcm_hcd->tp_data_dump_proc = NULL;
	tcm_hcd->tp_selftest_proc = NULL;

	for (idx = 0; idx < ARRAY_SIZE(dynamic_config_attrs); idx++) {
		sysfs_remove_file(tcm_hcd->dynamnic_config_sysfs_dir,
				&(*dynamic_config_attrs[idx]).attr);
	}

	kobject_put(tcm_hcd->dynamnic_config_sysfs_dir);

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++)
		sysfs_remove_file(tcm_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(tcm_hcd->sysfs_dir);

	if (bdata->irq_gpio >= 0)
		syna_tcm_set_gpio(tcm_hcd, bdata->irq_gpio, false, 0, 0);

	if (bdata->power_gpio >= 0)
		syna_tcm_set_gpio(tcm_hcd, bdata->power_gpio, false, 0, 0);

	if (bdata->reset_gpio >= 0)
		syna_tcm_set_gpio(tcm_hcd, bdata->reset_gpio, false, 0, 0);

	syna_tcm_enable_regulator(tcm_hcd, false);

	device_init_wakeup(&pdev->dev, 0);

	RELEASE_BUFFER(tcm_hcd->report.buffer);
	RELEASE_BUFFER(tcm_hcd->config);
	RELEASE_BUFFER(tcm_hcd->temp);
	RELEASE_BUFFER(tcm_hcd->resp);
	RELEASE_BUFFER(tcm_hcd->out);
	RELEASE_BUFFER(tcm_hcd->in);

	kfree(tcm_hcd);
	tcm_hcd = NULL;
	gloab_tcm_hcd = NULL;

	return 0;
}

static void syna_tcm_shutdown(struct platform_device *pdev)
{
	int retval;

	retval = syna_tcm_remove(pdev);
}

#ifdef CONFIG_PM
static int syna_pm_suspend(struct device *dev) {
	struct syna_tcm_hcd *tcm_hcd = dev_get_drvdata(dev);
	LOGN(tcm_hcd->pdev->dev.parent, "%s enter!\n", __func__);

	if (!tcm_hcd) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to enter syna_pm_suspend!\n");
		return -1;
	}
	tcm_hcd->tp_pm_suspend = true;
	reinit_completion(&tcm_hcd->pm_resume_completion);

	return 0;
}

static int syna_pm_resume(struct device *dev) {
	struct syna_tcm_hcd *tcm_hcd = dev_get_drvdata(dev);
	LOGN(tcm_hcd->pdev->dev.parent, "%s enter!\n", __func__);

	if (!tcm_hcd) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to enter syna_pm_resume!\n");
		return -1;
	}

	tcm_hcd->tp_pm_suspend = false;
	complete(&tcm_hcd->pm_resume_completion);

	return 0;
}

static const struct dev_pm_ops syna_tcm_dev_pm_ops = {
	.suspend = syna_pm_suspend,
	.resume = syna_pm_resume,
};
#endif

static struct platform_driver syna_tcm_driver = {
	.driver = {
		.name = PLATFORM_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &syna_tcm_dev_pm_ops,
#endif
	},
	.probe = syna_tcm_probe,
	.remove = syna_tcm_remove,
	.shutdown = syna_tcm_shutdown,
};

static int __init syna_tcm_module_init(void)
{
	int retval;
	int gpio_96;
	gpio_direction_input(K9E_ID_DET);
	gpio_96 = gpio_get_value(K9E_ID_DET);
	pr_info("gpio_96 = %d\n",gpio_96);
	if(gpio_96){
		pr_info("TP is synaptics");
	}else{
		pr_info("TP is goodix");
		return 0;
	}
	retval = syna_tcm_bus_init();
	if (retval < 0)
		return retval;

	return platform_driver_register(&syna_tcm_driver);
}

static void __exit syna_tcm_module_exit(void)
{
	platform_driver_unregister(&syna_tcm_driver);

	syna_tcm_bus_exit();

	return;
}

module_init(syna_tcm_module_init);
module_exit(syna_tcm_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics TCM Touch Driver");
MODULE_LICENSE("GPL v2");
