/*
 * Himax Android Driver Sample Code for common functions
 *
 * Copyright (C) 2018 Himax Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include "himax_common.h"
#include "himax_ic_core.h"
#include "himax_platform.h"

#ifdef CONFIG_TOUCHSCREEN_HIMAX_DEBUG
#include "himax_debug.h"
#endif

#define SUPPORT_FINGER_DATA_CHECKSUM 0x0F
#define TS_WAKE_LOCK_TIMEOUT (2 * HZ)
#define FRAME_COUNT 5
#define COMMON_BUF_SZ 80

#if defined(HX_AUTO_UPDATE_FW)
char *i_CTPM_firmware_name = "Himax_firmware.bin";
const struct firmware *i_CTPM_FW;
#endif

struct himax_ts_data *private_ts;
struct himax_ic_data *ic_data;
struct himax_report_data *hx_touch_data;
struct himax_core_fp g_core_fp;
struct himax_debug *debug_data;

struct proc_dir_entry *himax_touch_proc_dir;
#define HIMAX_PROC_TOUCH_FOLDER	"android_touch"

/* ts_work about start */
struct himax_target_report_data *g_target_report_data;
/* ts_work about end */

static int HX_TOUCH_INFO_POINT_CNT;

/* Himax: Set FW and CFG Flash Address */
#define FW_VER_MAJ_FLASH_ADDR  0x00C005
#define FW_VER_MIN_FLASH_ADDR  0x00C006
#define CFG_VER_MAJ_FLASH_ADDR 0x00C100
#define CFG_VER_MIN_FLASH_ADDR 0x00C101
#define CID_VER_MAJ_FLASH_ADDR 0x00C002
#define CID_VER_MIN_FLASH_ADDR 0x00C003

#define FW_VER_MAJ_FLASH_LENG  1
#define FW_VER_MIN_FLASH_LENG  1
#define CFG_VER_MAJ_FLASH_LENG 1
#define CFG_VER_MIN_FLASH_LENG 1
#define CID_VER_MAJ_FLASH_LENG 1
#define CID_VER_MIN_FLASH_LENG 1

unsigned long FW_CFG_VER_FLASH_ADDR;

unsigned char IC_CHECKSUM;

#ifdef HX_ESD_RECOVERY
	u8 HX_ESD_RESET_ACTIVATE = 0;
	int hx_EB_event_flag = 0;
	int hx_EC_event_flag = 0;
	int hx_ED_event_flag = 0;
	int g_zero_event_count = 0;
#endif
u8 HX_HW_RESET_ACTIVATE;

static uint8_t AA_press;
static uint8_t EN_NoiseFilter;
static uint8_t Last_EN_NoiseFilter;

static int p_point_num	= 0xFFFF;
#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
static uint8_t vk_press;
static int tpd_key;
static int tpd_key_old;
#endif
static int probe_fail_flag;
#ifdef HX_USB_DETECT_GLOBAL
	bool USB_detect_flag;
#endif


#if defined(CONFIG_DRM)
int drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#elif defined(CONFIG_FB)
int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void himax_ts_early_suspend(struct early_suspend *h);
static void himax_ts_late_resume(struct early_suspend *h);
#endif

#if defined(HX_PALM_REPORT)
	int himax_palm_detect(uint8_t *buf);
#endif

#ifdef HX_GESTURE_TRACK
	static int gest_pt_cnt;
	static int gest_pt_x[GEST_PT_MAX_NUM];
	static int gest_pt_y[GEST_PT_MAX_NUM];
	static int gest_start_x, gest_start_y, gest_end_x, gest_end_y;
	static int gest_width, gest_height, gest_mid_x, gest_mid_y;
	static int gn_gesture_coor[16];
#endif

int himax_report_data_init(void);

int g_ts_dbg;

/* File node for SMWP and HSEN - Start*/
uint8_t HX_PROC_SEND_FLAG;
#ifdef HX_SMART_WAKEUP
	#define HIMAX_PROC_SMWP_FILE "SMWP"
	struct proc_dir_entry *himax_proc_SMWP_file = NULL;
	#define HIMAX_PROC_GESTURE_FILE "GESTURE"
	struct proc_dir_entry *himax_proc_GESTURE_file = NULL;
	uint8_t HX_SMWP_EN = 0;
	bool FAKE_POWER_KEY_SEND;
#endif

#ifdef HX_HIGH_SENSE
	#define HIMAX_PROC_HSEN_FILE "HSEN"
	struct proc_dir_entry *himax_proc_HSEN_file = NULL;
#endif

#ifdef HX_HIGH_SENSE
static ssize_t himax_HSEN_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			E("%s: allocate memory failed!\n", __func__);
			return 0;
		}

		count = snprintf(temp_buf, len, "%d\n", ts->HSEN_enable);

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;

	return count;
}

static ssize_t himax_HSEN_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf[COMMON_BUF_SZ] = {0};

	if (len >= COMMON_BUF_SZ) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == '0')
		ts->HSEN_enable = 0;
	else if (buf[0] == '1')
		ts->HSEN_enable = 1;
	else
		return -EINVAL;

	g_core_fp.fp_set_HSEN_enable(ts->HSEN_enable, ts->suspended);
	D("%s: HSEN_enable = %d.\n", __func__, ts->HSEN_enable);
	return len;
}

static const struct file_operations himax_proc_HSEN_ops = {
	.owner = THIS_MODULE,
	.read = himax_HSEN_read,
	.write = himax_HSEN_write,
};
#endif

#ifdef HX_SMART_WAKEUP
static ssize_t himax_SMWP_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	size_t count = 0;
	struct himax_ts_data *ts = private_ts;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			E("%s: allocate memory failed!\n", __func__);
			return 0;
		}
		count = snprintf(temp_buf, len, "%d\n", ts->SMWP_enable);

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;

	return count;
}

static ssize_t himax_SMWP_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf[COMMON_BUF_SZ] = {0};

	if (len >= COMMON_BUF_SZ) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == '0')
		ts->SMWP_enable = 0;
	else if (buf[0] == '1')
		ts->SMWP_enable = 1;
	else
		return -EINVAL;

	g_core_fp.fp_set_SMWP_enable(ts->SMWP_enable, ts->suspended);
	HX_SMWP_EN = ts->SMWP_enable;
	I("%s: SMART_WAKEUP_enable = %d.\n", __func__, HX_SMWP_EN);
	return len;
}

static const struct file_operations himax_proc_SMWP_ops = {
	.owner = THIS_MODULE,
	.read = himax_SMWP_read,
	.write = himax_SMWP_write,
};

static ssize_t himax_GESTURE_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	int i = 0;
	size_t ret = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			E("%s: allocate memory failed!\n", __func__);
			return 0;
		}

		for (i = 0; i < 16; i++)
			ret += snprintf(temp_buf + ret, len - ret,
				"ges_en[%d]=%d\n", i, ts->gesture_cust_en[i]);

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else {
		HX_PROC_SEND_FLAG = 0;
		ret = 0;
	}

	return ret;
}

static ssize_t himax_GESTURE_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	int i = 0;
	char buf[COMMON_BUF_SZ] = {0};

	if (len >= COMMON_BUF_SZ) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	I("himax_GESTURE_store= %s\n", buf);

	for (i = 0; i < 16; i++) {
		if (buf[i] == '0')
			ts->gesture_cust_en[i] = 0;
		else if (buf[i] == '1')
			ts->gesture_cust_en[i] = 1;
		else
			ts->gesture_cust_en[i] = 0;

		I("gesture en[%d]=%d\n", i, ts->gesture_cust_en[i]);
	}

	return len;
}

static const struct file_operations himax_proc_Gesture_ops = {
	.owner = THIS_MODULE,
	.read = himax_GESTURE_read,
	.write = himax_GESTURE_write,
};
#endif

#ifdef CONFIG_TOUCHSCREEN_HIMAX_INSPECT
extern void (*fp_himax_self_test_init)(void);
#endif

int himax_common_proc_init(void)
{
	himax_touch_proc_dir = proc_mkdir(HIMAX_PROC_TOUCH_FOLDER, NULL);

	if (himax_touch_proc_dir == NULL) {
		E(" %s: himax_touch_proc_dir file create failed!\n", __func__);
		return -ENOMEM;
	}
#ifdef CONFIG_TOUCHSCREEN_HIMAX_INSPECT
	if (fp_himax_self_test_init != NULL)
		fp_himax_self_test_init();
#endif

#ifdef HX_HIGH_SENSE
	himax_proc_HSEN_file = proc_create(HIMAX_PROC_HSEN_FILE, 0666,
				himax_touch_proc_dir, &himax_proc_HSEN_ops);

	if (himax_proc_HSEN_file == NULL) {
		E(" %s: proc HSEN file create failed!\n", __func__);
		goto fail_1;
	}

#endif
#ifdef HX_SMART_WAKEUP
	himax_proc_SMWP_file = proc_create(HIMAX_PROC_SMWP_FILE, 0666,
				himax_touch_proc_dir, &himax_proc_SMWP_ops);

	if (himax_proc_SMWP_file == NULL) {
		E(" %s: proc SMWP file create failed!\n", __func__);
		goto fail_2;
	}

	himax_proc_GESTURE_file = proc_create(HIMAX_PROC_GESTURE_FILE, 0666,
			himax_touch_proc_dir, &himax_proc_Gesture_ops);

	if (himax_proc_GESTURE_file == NULL) {
		E(" %s: proc GESTURE file create failed!\n", __func__);
		goto fail_3;
	}
#endif
	return 0;

#ifdef HX_SMART_WAKEUP
	remove_proc_entry(HIMAX_PROC_GESTURE_FILE, himax_touch_proc_dir);
fail_3:
	remove_proc_entry(HIMAX_PROC_SMWP_FILE, himax_touch_proc_dir);
fail_2:
#endif
#ifdef HX_HIGH_SENSE
	remove_proc_entry(HIMAX_PROC_HSEN_FILE, himax_touch_proc_dir);
fail_1:
#endif
	return -ENOMEM;
}

void himax_common_proc_deinit(void)
{
#ifdef HX_SMART_WAKEUP
	remove_proc_entry(HIMAX_PROC_GESTURE_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_SMWP_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_HIGH_SENSE
	remove_proc_entry(HIMAX_PROC_HSEN_FILE, himax_touch_proc_dir);
#endif
}

/* File node for SMWP and HSEN - End*/

int himax_input_register(struct himax_ts_data *ts)
{
	int ret = 0;

	ret = himax_dev_set(ts);
	if (ret < 0)
		goto input_device_fail;

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
#if defined(HX_PLATFOME_DEFINE_KEY)
	himax_platform_key();
#else
	set_bit(KEY_BACK, ts->input_dev->keybit);
	set_bit(KEY_HOME, ts->input_dev->keybit);
	set_bit(KEY_MENU, ts->input_dev->keybit);
	set_bit(KEY_SEARCH, ts->input_dev->keybit);
#endif
#if defined(HX_SMART_WAKEUP) || defined(HX_PALM_REPORT)
	set_bit(KEY_POWER, ts->input_dev->keybit);
#endif
#if defined(HX_SMART_WAKEUP)
	set_bit(KEY_CUST_01, ts->input_dev->keybit);
	set_bit(KEY_CUST_02, ts->input_dev->keybit);
	set_bit(KEY_CUST_03, ts->input_dev->keybit);
	set_bit(KEY_CUST_04, ts->input_dev->keybit);
	set_bit(KEY_CUST_05, ts->input_dev->keybit);
	set_bit(KEY_CUST_06, ts->input_dev->keybit);
	set_bit(KEY_CUST_07, ts->input_dev->keybit);
	set_bit(KEY_CUST_08, ts->input_dev->keybit);
	set_bit(KEY_CUST_09, ts->input_dev->keybit);
	set_bit(KEY_CUST_10, ts->input_dev->keybit);
	set_bit(KEY_CUST_11, ts->input_dev->keybit);
	set_bit(KEY_CUST_12, ts->input_dev->keybit);
	set_bit(KEY_CUST_13, ts->input_dev->keybit);
	set_bit(KEY_CUST_14, ts->input_dev->keybit);
	set_bit(KEY_CUST_15, ts->input_dev->keybit);
#endif
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(KEY_APPSELECT, ts->input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
#ifdef	HX_PROTOCOL_A
	/* ts->input_dev->mtsize = ts->nFinger_support; */
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 3, 0, 0);
#else
	set_bit(MT_TOOL_FINGER, ts->input_dev->keybit);
#if defined(HX_PROTOCOL_B_3PA)
	input_mt_init_slots(ts->input_dev, ts->nFinger_support, INPUT_MT_DIRECT);
#else
	input_mt_init_slots(ts->input_dev, ts->nFinger_support);
#endif
#endif
	D("input_set_abs_params: mix_x %d, max_x %d, min_y %d, max_y %d\n",
		ts->pdata->abs_x_min, ts->pdata->abs_x_max,
		ts->pdata->abs_y_min, ts->pdata->abs_y_max);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_x_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, ts->pdata->abs_y_min, ts->pdata->abs_y_max, ts->pdata->abs_y_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, ts->pdata->abs_pressure_min, ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
#ifndef	HX_PROTOCOL_A
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, ts->pdata->abs_pressure_min, ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, ts->pdata->abs_width_min, ts->pdata->abs_width_max, ts->pdata->abs_pressure_fuzz, 0);
#endif
	/* input_set_abs_params(ts->input_dev, ABS_MT_AMPLITUDE, 0, ((ts->pdata->abs_pressure_max << 16) | ts->pdata->abs_width_max), 0, 0); */
	/* input_set_abs_params(ts->input_dev, ABS_MT_POSITION, 0, (BIT(31) | (ts->pdata->abs_x_max << 16) | ts->pdata->abs_y_max), 0, 0); */

	if (himax_input_register_device(ts->input_dev) == 0)
		return NO_ERR;

	ret = INPUT_REGISTER_FAIL;

input_device_fail:
	E("%s, input device register fail\n", __func__);
	return ret;
}

static void calcDataSize(uint8_t finger_num)
{
	struct himax_ts_data *ts_data = private_ts;

	ts_data->coord_data_size = 4 * finger_num;
	ts_data->area_data_size = ((finger_num / 4) + (finger_num % 4 ? 1 : 0)) * 4;
	ts_data->coordInfoSize = ts_data->coord_data_size + ts_data->area_data_size + 4;
	ts_data->raw_data_frame_size = 128 - ts_data->coord_data_size - ts_data->area_data_size - 4 - 4 - 1;

	if (ts_data->raw_data_frame_size == 0) {
		E("%s: could NOT calculate!\n", __func__);
		return;
	}

	ts_data->raw_data_nframes  = ((uint32_t)ts_data->x_channel * ts_data->y_channel +
		ts_data->x_channel + ts_data->y_channel) / ts_data->raw_data_frame_size +
		(((uint32_t)ts_data->x_channel * ts_data->y_channel +
		ts_data->x_channel + ts_data->y_channel) % ts_data->raw_data_frame_size) ? 1 : 0;

	D("%s: coord_data_size:%d, area_data_size:%d\n", __func__,
		ts_data->coord_data_size, ts_data->area_data_size);
	D("%s: raw_data_frame_size:%d, raw_data_nframes:%d\n", __func__,
		ts_data->raw_data_frame_size, ts_data->raw_data_nframes);
}

static void calculate_point_number(void)
{
	HX_TOUCH_INFO_POINT_CNT = ic_data->HX_MAX_PT * 4;

	if ((ic_data->HX_MAX_PT % 4) == 0)
		HX_TOUCH_INFO_POINT_CNT += (ic_data->HX_MAX_PT / 4) * 4;
	else
		HX_TOUCH_INFO_POINT_CNT += ((ic_data->HX_MAX_PT / 4) + 1) * 4;
}

#ifdef HX_AUTO_UPDATE_FW
static int i_update_FW(void)
{
	int upgrade_times = 0;
	unsigned char *ImageBuffer = NULL;
	int fullFileLength = 0;
	int ret = 0, result = 0;
	uint32_t i_FW_VER = 0, i_CFG_VER = 0;

	D("file name = %s\n", i_CTPM_firmware_name);
	if (request_firmware(&i_CTPM_FW, i_CTPM_firmware_name,
		private_ts->dev)) {
		I("%s: no firmware file\n", __func__);
		return OPEN_FILE_FAIL;
	}

	fullFileLength = i_CTPM_FW->size;
	ImageBuffer = (unsigned char *)i_CTPM_FW->data;

	i_FW_VER = (ImageBuffer[FW_VER_MAJ_FLASH_ADDR] << 8)
			| ImageBuffer[FW_VER_MIN_FLASH_ADDR];
	i_CFG_VER = (ImageBuffer[CFG_VER_MAJ_FLASH_ADDR] << 8)
			| ImageBuffer[CFG_VER_MIN_FLASH_ADDR];

	if ((ic_data->vendor_fw_ver >= i_FW_VER)
		&& (ic_data->vendor_config_ver >= i_CFG_VER)) {
		D("FW_VER 0x%x, CFG_VER 0x%x\n", i_FW_VER, i_CFG_VER);
		release_firmware(i_CTPM_FW);
		return 0;
	}

	himax_int_enable(0);
update_retry:

	if (fullFileLength == FW_SIZE_32k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_32k(ImageBuffer, fullFileLength, false);
	else if (fullFileLength == FW_SIZE_60k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_60k(ImageBuffer, fullFileLength, false);
	else if (fullFileLength == FW_SIZE_64k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k(ImageBuffer, fullFileLength, false);
	else if (fullFileLength == FW_SIZE_124k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_124k(ImageBuffer, fullFileLength, false);
	else if (fullFileLength == FW_SIZE_128k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k(ImageBuffer, fullFileLength, false);

	release_firmware(i_CTPM_FW);

	if (ret == 0) {
		upgrade_times++;
		E("%s: TP upgrade error, upgrade_times = %d\n", __func__, upgrade_times);

		if (upgrade_times < 3)
			goto update_retry;
		else
			result = -1; /* upgrade fail */
	} else {
		g_core_fp.fp_read_FW_ver();
		g_core_fp.fp_touch_information();
		result = 1;/* upgrade success */
		D("%s: TP upgrade OK\n", __func__);
	}

#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(true, false);
#else
	g_core_fp.fp_sense_on(0x00);
#endif
	himax_int_enable(1);
	return result;
}
#endif

static int himax_loadSensorConfig(struct himax_i2c_platform_data *pdata)
{
	D("%s: initialization complete\n", __func__);
	return NO_ERR;
}

#ifdef HX_ESD_RECOVERY
static void himax_esd_hw_reset(void)
{
	if (g_ts_dbg != 0)
		I("%s: Entering\n", __func__);

	I("START_Himax TP: ESD - Reset\n");

	if (private_ts->in_self_test == 1) {
		I("In self test , not  TP: ESD - Reset\n");
		return;
	}

	g_core_fp.fp_esd_ic_reset();
	I("END_Himax TP: ESD - Reset\n");
}
#endif

#ifdef HX_SMART_WAKEUP
#ifdef HX_GESTURE_TRACK
static void gest_pt_log_coordinate(int rx, int tx)
{
	/* driver report x y with range 0 - 255 , we scale it up to x/y pixel */
	gest_pt_x[gest_pt_cnt] = rx * (ic_data->HX_X_RES) / 255;
	gest_pt_y[gest_pt_cnt] = tx * (ic_data->HX_Y_RES) / 255;
}
#endif
static int himax_wake_event_parse(struct himax_ts_data *ts, int ts_status)
{
	uint8_t *buf;
#ifdef HX_GESTURE_TRACK
	int tmp_max_x = 0x00, tmp_min_x = 0xFFFF, tmp_max_y = 0x00, tmp_min_y = 0xFFFF;
	int gest_len;
#endif
	int i = 0, check_FC = 0, gesture_flag = 0;

	if (g_ts_dbg != 0)
		I("%s: Entering!, ts_status=%d\n", __func__, ts_status);

	buf = kcalloc(hx_touch_data->event_size, sizeof(uint8_t), GFP_KERNEL);
	if (!buf) {
		E("%s: allocate memory failed!\n", __func__);
		return 0;
	}
	memcpy(buf, hx_touch_data->hx_event_buf, hx_touch_data->event_size);

	for (i = 0; i < GEST_PTLG_ID_LEN; i++) {
		if (check_FC == 0) {
			if ((buf[0] != 0x00) && ((buf[0] <= 0x0F) || (buf[0] == 0x80))) {
				check_FC = 1;
				gesture_flag = buf[i];
			} else {
				check_FC = 0;
				I("ID START at %x , value = %x skip the event\n", i, buf[i]);
				break;
			}
		} else {
			if (buf[i] != gesture_flag) {
				check_FC = 0;
				I("ID NOT the same %x != %x So STOP parse event\n", buf[i], gesture_flag);
				break;
			}
		}

		I("0x%2.2X ", buf[i]);

		if (i % 8 == 7)
			I("\n");
	}

	I("Himax gesture_flag= %x\n", gesture_flag);
	I("Himax check_FC is %d\n", check_FC);

	if (check_FC == 0)
		return 0;

	if (buf[GEST_PTLG_ID_LEN] != GEST_PTLG_HDR_ID1 ||
	    buf[GEST_PTLG_ID_LEN + 1] != GEST_PTLG_HDR_ID2)
		return 0;

#ifdef HX_GESTURE_TRACK

	if (buf[GEST_PTLG_ID_LEN] == GEST_PTLG_HDR_ID1 &&
	    buf[GEST_PTLG_ID_LEN + 1] == GEST_PTLG_HDR_ID2) {
		gest_len = buf[GEST_PTLG_ID_LEN + 2];
		I("gest_len = %d ", gest_len);
		i = 0;
		gest_pt_cnt = 0;
		I("gest doornidate start\n %s", __func__);

		while (i < (gest_len + 1) / 2) {
			gest_pt_log_coordinate(buf[GEST_PTLG_ID_LEN + 4 + i * 2], buf[GEST_PTLG_ID_LEN + 4 + i * 2 + 1]);
			i++;
			I("gest_pt_x[%d]=%d\n", gest_pt_cnt, gest_pt_x[gest_pt_cnt]);
			I("gest_pt_y[%d]=%d\n", gest_pt_cnt, gest_pt_y[gest_pt_cnt]);
			gest_pt_cnt += 1;
		}

		if (gest_pt_cnt) {
			for (i = 0; i < gest_pt_cnt; i++) {
				if (tmp_max_x < gest_pt_x[i])
					tmp_max_x = gest_pt_x[i];

				if (tmp_min_x > gest_pt_x[i])
					tmp_min_x = gest_pt_x[i];

				if (tmp_max_y < gest_pt_y[i])
					tmp_max_y = gest_pt_y[i];

				if (tmp_min_y > gest_pt_y[i])
					tmp_min_y = gest_pt_y[i];
			}

			I("gest_point x_min= %d, x_max= %d, y_min= %d, y_max= %d\n", tmp_min_x, tmp_max_x, tmp_min_y, tmp_max_y);
			gest_start_x = gest_pt_x[0];
			gn_gesture_coor[0] = gest_start_x;
			gest_start_y = gest_pt_y[0];
			gn_gesture_coor[1] = gest_start_y;
			gest_end_x = gest_pt_x[gest_pt_cnt - 1];
			gn_gesture_coor[2] = gest_end_x;
			gest_end_y = gest_pt_y[gest_pt_cnt - 1];
			gn_gesture_coor[3] = gest_end_y;
			gest_width = tmp_max_x - tmp_min_x;
			gn_gesture_coor[4] = gest_width;
			gest_height = tmp_max_y - tmp_min_y;
			gn_gesture_coor[5] = gest_height;
			gest_mid_x = (tmp_max_x + tmp_min_x) / 2;
			gn_gesture_coor[6] = gest_mid_x;
			gest_mid_y = (tmp_max_y + tmp_min_y) / 2;
			gn_gesture_coor[7] = gest_mid_y;
			gn_gesture_coor[8] = gest_mid_x; /* gest_up_x */
			gn_gesture_coor[9] = gest_mid_y - gest_height / 2; /* gest_up_y */
			gn_gesture_coor[10] = gest_mid_x; /* gest_down_x */
			gn_gesture_coor[11] = gest_mid_y + gest_height / 2; /* gest_down_y */
			gn_gesture_coor[12] = gest_mid_x - gest_width / 2; /* gest_left_x */
			gn_gesture_coor[13] = gest_mid_y; /* gest_left_y */
			gn_gesture_coor[14] = gest_mid_x + gest_width / 2; /* gest_right_x */
			gn_gesture_coor[15] = gest_mid_y; /* gest_right_y */
		}
	}

#endif

	if (gesture_flag != 0x80) {
		if (!ts->gesture_cust_en[gesture_flag]) {
			I("%s NOT report customer key\n ", __func__);
			g_target_report_data->SMWP_event_chk = 0;
			return 0;/* NOT report customer key */
		}
	} else {
		if (!ts->gesture_cust_en[0]) {
			I("%s NOT report report double click\n", __func__);
			g_target_report_data->SMWP_event_chk = 0;
			return 0;/* NOT report power key */
		}
	}

	if (gesture_flag == 0x80) {
		g_target_report_data->SMWP_event_chk = EV_GESTURE_PWR;
		return EV_GESTURE_PWR;
	}

	g_target_report_data->SMWP_event_chk = gesture_flag;
	return gesture_flag;
}

static void himax_wake_event_report(void)
{
	int ret_event = g_target_report_data->SMWP_event_chk;
	int KEY_EVENT = 0;

	if (g_ts_dbg != 0)
		I("%s: Entering!\n", __func__);

	switch (ret_event) {
	case EV_GESTURE_PWR:
		KEY_EVENT = KEY_POWER;
		break;

	case EV_GESTURE_01:
		KEY_EVENT = KEY_CUST_01;
		break;

	case EV_GESTURE_02:
		KEY_EVENT = KEY_CUST_02;
		break;

	case EV_GESTURE_03:
		KEY_EVENT = KEY_CUST_03;
		break;

	case EV_GESTURE_04:
		KEY_EVENT = KEY_CUST_04;
		break;

	case EV_GESTURE_05:
		KEY_EVENT = KEY_CUST_05;
		break;

	case EV_GESTURE_06:
		KEY_EVENT = KEY_CUST_06;
		break;

	case EV_GESTURE_07:
		KEY_EVENT = KEY_CUST_07;
		break;

	case EV_GESTURE_08:
		KEY_EVENT = KEY_CUST_08;
		break;

	case EV_GESTURE_09:
		KEY_EVENT = KEY_CUST_09;
		break;

	case EV_GESTURE_10:
		KEY_EVENT = KEY_CUST_10;
		break;

	case EV_GESTURE_11:
		KEY_EVENT = KEY_CUST_11;
		break;

	case EV_GESTURE_12:
		KEY_EVENT = KEY_CUST_12;
		break;

	case EV_GESTURE_13:
		KEY_EVENT = KEY_CUST_13;
		break;

	case EV_GESTURE_14:
		KEY_EVENT = KEY_CUST_14;
		break;

	case EV_GESTURE_15:
		KEY_EVENT = KEY_CUST_15;
		break;
	}

	if (ret_event) {
		I(" %s SMART WAKEUP KEY event %x press\n", __func__, KEY_EVENT);
		input_report_key(private_ts->input_dev, KEY_EVENT, 1);
		input_sync(private_ts->input_dev);
		I(" %s SMART WAKEUP KEY event %x release\n", __func__, KEY_EVENT);
		input_report_key(private_ts->input_dev, KEY_EVENT, 0);
		input_sync(private_ts->input_dev);
		FAKE_POWER_KEY_SEND = true;
#ifdef HX_GESTURE_TRACK
		I("gest_start_x= %d, gest_start_y= %d, gest_end_x= %d, gest_end_y= %d\n", gest_start_x, gest_start_y,
		  gest_end_x, gest_end_y);
		I("gest_width= %d, gest_height= %d, gest_mid_x= %d, gest_mid_y= %d\n", gest_width, gest_height,
		  gest_mid_x, gest_mid_y);
		I("gest_up_x= %d, gest_up_y= %d, gest_down_x= %d, gest_down_y= %d\n", gn_gesture_coor[8], gn_gesture_coor[9],
		  gn_gesture_coor[10], gn_gesture_coor[11]);
		I("gest_left_x= %d, gest_left_y= %d, gest_right_x= %d, gest_right_y= %d\n", gn_gesture_coor[12], gn_gesture_coor[13],
		  gn_gesture_coor[14], gn_gesture_coor[15]);
#endif
		g_target_report_data->SMWP_event_chk = 0;
	}
}

#endif

int himax_report_data_init(void)
{
	if (hx_touch_data->hx_coord_buf != NULL)
		kfree(hx_touch_data->hx_coord_buf);

	if (hx_touch_data->hx_rawdata_buf != NULL)
		kfree(hx_touch_data->hx_rawdata_buf);

#if defined(HX_SMART_WAKEUP)
	hx_touch_data->event_size = g_core_fp.fp_get_touch_data_size();

	if (hx_touch_data->hx_event_buf != NULL)
		kfree(hx_touch_data->hx_event_buf);

#endif
	hx_touch_data->touch_all_size = g_core_fp.fp_get_touch_data_size();
	hx_touch_data->raw_cnt_max = ic_data->HX_MAX_PT / 4;
	hx_touch_data->raw_cnt_rmd = ic_data->HX_MAX_PT % 4;
	/* more than 4 fingers */
	if (hx_touch_data->raw_cnt_rmd != 0x00) {
		hx_touch_data->rawdata_size = g_core_fp.fp_cal_data_len(hx_touch_data->raw_cnt_rmd, ic_data->HX_MAX_PT, hx_touch_data->raw_cnt_max);
		hx_touch_data->touch_info_size = (ic_data->HX_MAX_PT + hx_touch_data->raw_cnt_max + 2) * 4;
	} else { /* less than 4 fingers */
		hx_touch_data->rawdata_size = g_core_fp.fp_cal_data_len(hx_touch_data->raw_cnt_rmd, ic_data->HX_MAX_PT, hx_touch_data->raw_cnt_max);
		hx_touch_data->touch_info_size = (ic_data->HX_MAX_PT + hx_touch_data->raw_cnt_max + 1) * 4;
	}

	if ((ic_data->HX_TX_NUM * ic_data->HX_RX_NUM + ic_data->HX_TX_NUM + ic_data->HX_RX_NUM) % hx_touch_data->rawdata_size == 0)
		hx_touch_data->rawdata_frame_size = (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM + ic_data->HX_TX_NUM + ic_data->HX_RX_NUM) / hx_touch_data->rawdata_size;
	else
		hx_touch_data->rawdata_frame_size = (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM + ic_data->HX_TX_NUM + ic_data->HX_RX_NUM) / hx_touch_data->rawdata_size + 1;


	D("%s: rawdata_frame_size = %d",
		__func__, hx_touch_data->rawdata_frame_size);

	D("%s: ic_data->HX_MAX_PT:%d, hx_raw_cnt_max:%d, hx_raw_cnt_rmd:%d\n",
		__func__, ic_data->HX_MAX_PT, hx_touch_data->raw_cnt_max,
		hx_touch_data->raw_cnt_rmd);
	D("%s: g_hx_rawdata_size:%d, hx_touch_data->touch_info_size:%d\n",
		__func__, hx_touch_data->rawdata_size,
		hx_touch_data->touch_info_size);
	hx_touch_data->hx_coord_buf = kzalloc(sizeof(uint8_t) * (hx_touch_data->touch_info_size), GFP_KERNEL);

	if (hx_touch_data->hx_coord_buf == NULL)
		goto mem_alloc_fail;

	g_target_report_data = kzalloc(sizeof(struct himax_target_report_data), GFP_KERNEL);
	if (g_target_report_data == NULL)
		goto mem_alloc_fail;
	g_target_report_data->x = kzalloc(sizeof(int)*(ic_data->HX_MAX_PT), GFP_KERNEL);
	if (g_target_report_data->x == NULL)
		goto mem_alloc_fail;
	g_target_report_data->y = kzalloc(sizeof(int)*(ic_data->HX_MAX_PT), GFP_KERNEL);
	if (g_target_report_data->y == NULL)
		goto mem_alloc_fail;
	g_target_report_data->w = kzalloc(sizeof(int)*(ic_data->HX_MAX_PT), GFP_KERNEL);
	if (g_target_report_data->w == NULL)
		goto mem_alloc_fail;
	g_target_report_data->finger_id = kzalloc(sizeof(int)*(ic_data->HX_MAX_PT), GFP_KERNEL);
	if (g_target_report_data->finger_id == NULL)
		goto mem_alloc_fail;
#ifdef HX_SMART_WAKEUP
	g_target_report_data->SMWP_event_chk = 0;
#endif

	hx_touch_data->hx_rawdata_buf = kzalloc(sizeof(uint8_t) * (hx_touch_data->touch_all_size - hx_touch_data->touch_info_size), GFP_KERNEL);

	if (hx_touch_data->hx_rawdata_buf == NULL)
		goto mem_alloc_fail;

#if defined(HX_SMART_WAKEUP)
	hx_touch_data->hx_event_buf = kzalloc(sizeof(uint8_t) * (hx_touch_data->event_size), GFP_KERNEL);

	if (hx_touch_data->hx_event_buf == NULL)
		goto mem_alloc_fail;

#endif
	return NO_ERR;
mem_alloc_fail:
	kfree(hx_touch_data->hx_coord_buf);
	kfree(g_target_report_data->x);
	kfree(g_target_report_data->y);
	kfree(g_target_report_data->w);
	kfree(g_target_report_data->finger_id);
	kfree(hx_touch_data->hx_rawdata_buf);

#if defined(HX_SMART_WAKEUP)
	kfree(hx_touch_data->hx_event_buf);
#endif
	I("%s: Memory allocate fail!\n", __func__);
	return MEM_ALLOC_FAIL;
}

/* start ts_work */
#if defined(HX_USB_DETECT_GLOBAL)
void himax_cable_detect_func(bool force_renew)
{
	struct himax_ts_data *ts;
	u32 connect_status = 0;

	connect_status = USB_detect_flag;/* upmu_is_chr_det(); */
	ts = private_ts;

	if (ts->cable_config) {
		if (((!!connect_status) != ts->usb_connected) || force_renew) {
			if (!!connect_status) {
				ts->cable_config[1] = 0x01;
				ts->usb_connected = 0x01;
			} else {
				ts->cable_config[1] = 0x00;
				ts->usb_connected = 0x00;
			}

			g_core_fp.fp_usb_detect_set(ts->cable_config);
			I("%s: Cable status change: 0x%2.2X\n", __func__, ts->usb_connected);
		}

		/* else */
		/*	I("%s: Cable status is the same as previous one, ignore.\n", __func__); */
	}
}
#endif

static int himax_ts_work_status(struct himax_ts_data *ts)
{
	/* 1: normal, 2:SMWP */
	int result = HX_REPORT_COORD;

	hx_touch_data->diag_cmd = ts->diag_cmd;
	if (hx_touch_data->diag_cmd)
		result = HX_REPORT_COORD_RAWDATA;

#ifdef HX_SMART_WAKEUP
	if (atomic_read(&ts->suspend_mode) && (!FAKE_POWER_KEY_SEND) && (ts->SMWP_enable) && (!hx_touch_data->diag_cmd))
		result = HX_REPORT_SMWP_EVENT;
#endif
	/* I("Now Status is %d\n", result); */
	return result;
}

static int himax_touch_get(struct himax_ts_data *ts, uint8_t *buf, int ts_path, int ts_status)
{

	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	switch (ts_path) {
	/* normal */
	case HX_REPORT_COORD:
		if ((HX_HW_RESET_ACTIVATE)
#ifdef HX_ESD_RECOVERY
			|| (HX_ESD_RESET_ACTIVATE)
#endif
			) {
			if (!g_core_fp.fp_read_event_stack(buf, HX_REPORT_SZ)) {
				E("%s: can't read data from chip!\n", __func__);
				ts_status = HX_TS_GET_DATA_FAIL;
				goto END_FUNCTION;
			}
		} else {
			if (!g_core_fp.fp_read_event_stack(buf, hx_touch_data->touch_info_size)) {
				E("%s: can't read data from chip!\n", __func__);
				ts_status = HX_TS_GET_DATA_FAIL;
				goto END_FUNCTION;
			}
		}
		break;
#if defined(HX_SMART_WAKEUP)

	/* SMWP */
	case HX_REPORT_SMWP_EVENT:
		g_core_fp.fp_burst_enable(0);

		if (!g_core_fp.fp_read_event_stack(buf, hx_touch_data->event_size)) {
			E("%s: can't read data from chip!\n", __func__);
			ts_status = HX_TS_GET_DATA_FAIL;
			goto END_FUNCTION;
		}
		break;
#endif
	case HX_REPORT_COORD_RAWDATA:
		if (!g_core_fp.fp_read_event_stack(buf, HX_REPORT_SZ)) {
			E("%s: can't read data from chip!\n", __func__);
			ts_status = HX_TS_GET_DATA_FAIL;
			goto END_FUNCTION;
		}
		break;
	default:
		break;
	}

END_FUNCTION:
	return ts_status;
}

/* start error_control*/
static int himax_checksum_cal(struct himax_ts_data *ts, uint8_t *buf, int ts_path, int ts_status)
{
	uint16_t check_sum_cal = 0;
	int32_t	i = 0;
	int length = 0;
	int ret_val = ts_status;

	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	/* Normal */
	switch (ts_path) {
	case	HX_REPORT_COORD:
		length = hx_touch_data->touch_info_size;
		break;
#if defined(HX_SMART_WAKEUP)
/* SMWP */
	case HX_REPORT_SMWP_EVENT:
		length = (GEST_PTLG_ID_LEN + GEST_PTLG_HDR_LEN);
		break;
#endif
	case HX_REPORT_COORD_RAWDATA:
		length = hx_touch_data->touch_info_size;
		break;
	default:
		I("%s, Neither Normal Nor SMWP error!\n", __func__);
		ret_val = HX_PATH_FAIL;
		goto END_FUNCTION;

	}

	for (i = 0; i < length; i++)
		check_sum_cal += buf[i];
	if (check_sum_cal % 0x100 != 0) {
		I("[HIMAX TP MSG] checksum fail : check_sum_cal: 0x%02X\n", check_sum_cal);
		ret_val = HX_CHKSUM_FAIL;
	}

END_FUNCTION:
	if (g_ts_dbg != 0)
		I("%s: END, ret_val=%d!\n", __func__, ret_val);
	return ret_val;
}

#ifdef HX_ESD_RECOVERY
static int himax_ts_event_check(struct himax_ts_data *ts, uint8_t *buf, int ts_path, int ts_status)
{

	int hx_EB_event = 0;
	int hx_EC_event = 0;
	int hx_ED_event = 0;
	int hx_esd_event = 0;
	int hx_zero_event = 0;
	int shaking_ret = 0;

	int32_t	loop_i = 0;
	int length = 0;
	int ret_val = ts_status;

	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	/* Normal */
	switch (ts_path) {
	case HX_REPORT_COORD:
		length = hx_touch_data->touch_info_size;
		break;
#if defined(HX_SMART_WAKEUP)
	/* SMWP */
	case HX_REPORT_SMWP_EVENT:
		length = (GEST_PTLG_ID_LEN + GEST_PTLG_HDR_LEN);
		break;
#endif
	case HX_REPORT_COORD_RAWDATA:
		length = hx_touch_data->touch_info_size;
		break;
	default:
		I("%s, Neither Normal Nor SMWP error!\n", __func__);
		ret_val = HX_PATH_FAIL;
		goto END_FUNCTION;
	}

	if (g_ts_dbg != 0)
		I("Now Path=%d, Now status=%d, length=%d\n", ts_path, ts_status, length);

	for (loop_i = 0; loop_i < length; loop_i++) {
		if (ts_path == HX_REPORT_COORD || ts_path == HX_REPORT_COORD_RAWDATA) {
			/* case 1 ESD recovery flow */
			if (buf[loop_i] == 0xEB)
				hx_EB_event++;

			else if (buf[loop_i] == 0xEC)
				hx_EC_event++;

			else if (buf[loop_i] == 0xED)
				hx_ED_event++;

			/* case 2 ESD recovery flow-Disable */
			else if (buf[loop_i] == 0x00)
				hx_zero_event++;
			else {
				hx_EB_event = 0;
				hx_EC_event = 0;
				hx_ED_event = 0;
				hx_zero_event = 0;
				g_zero_event_count = 0;
			}
		}
	}

	if (hx_EB_event == length) {
		hx_esd_event = length;
		hx_EB_event_flag++;
		I("[HIMAX TP MSG]: ESD event checked - ALL 0xEB.\n");
	} else if (hx_EC_event == length) {
		hx_esd_event = length;
		hx_EC_event_flag++;
		I("[HIMAX TP MSG]: ESD event checked - ALL 0xEC.\n");
	} else if (hx_ED_event == length) {
		hx_esd_event = length;
		hx_ED_event_flag++;
		I("[HIMAX TP MSG]: ESD event checked - ALL 0xED.\n");
	} else {
		hx_esd_event = 0;
	}

	if ((hx_esd_event == length || hx_zero_event == length)
		&& (HX_HW_RESET_ACTIVATE == 0)
		&& (HX_ESD_RESET_ACTIVATE == 0)
		&& (hx_touch_data->diag_cmd == 0)
		&& (ts->in_self_test == 0)) {

		shaking_ret = g_core_fp.fp_ic_esd_recovery(hx_esd_event, hx_zero_event, length);

		if (shaking_ret == HX_ESD_EVENT) {
			himax_esd_hw_reset();
			ret_val = HX_ESD_EVENT;
		} else if (shaking_ret == HX_ZERO_EVENT_COUNT)
			ret_val = HX_ZERO_EVENT_COUNT;
		else {
			I("I2C running. Nothing to be done!\n");
			ret_val = HX_IC_RUNNING;
		}
	} else if (HX_ESD_RESET_ACTIVATE) {
		/* drop 1st interrupts after chip reset */
		HX_ESD_RESET_ACTIVATE = 0;
		I("[HX_ESD_RESET_ACTIVATE]:%s: Back from reset, ready to serve.\n", __func__);
		ret_val = HX_ESD_REC_OK;
	}

END_FUNCTION:
	if (g_ts_dbg != 0)
		I("%s: END, ret_val=%d!\n", __func__, ret_val);

	return ret_val;
}
#endif

static int himax_err_ctrl(struct himax_ts_data *ts, uint8_t *buf, int ts_path, int ts_status)
{

#ifdef HX_RST_PIN_FUNC
	if (HX_HW_RESET_ACTIVATE) {
		/* drop 1st interrupts after chip reset */
		HX_HW_RESET_ACTIVATE = 0;
		D(":%s: Back from reset, ready to serve.\n", __func__);
		ts_status = HX_RST_OK;
		goto END_FUNCTION;
	}
#endif

	ts_status = himax_checksum_cal(ts, buf, ts_path, ts_status);
	if (ts_status == HX_CHKSUM_FAIL)
		goto CHK_FAIL;
	goto END_FUNCTION;

CHK_FAIL:
#ifdef HX_ESD_RECOVERY
	ts_status = himax_ts_event_check(ts, buf, ts_path, ts_status);
#endif


END_FUNCTION:
	if (g_ts_dbg != 0)
		I("%s: END, ts_status=%d!\n", __func__, ts_status);
	return ts_status;
}
/* end error_control*/

/* start distribute_data*/
static int himax_distribute_touch_data(uint8_t *buf, int ts_path, int ts_status)
{
	uint8_t hx_state_info_pos = hx_touch_data->touch_info_size - 3;

	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	if (ts_path == HX_REPORT_COORD) {
		memcpy(hx_touch_data->hx_coord_buf, &buf[0], hx_touch_data->touch_info_size);

		if (buf[hx_state_info_pos] != 0xFF && buf[hx_state_info_pos + 1] != 0xFF)
			memcpy(hx_touch_data->hx_state_info, &buf[hx_state_info_pos], 2);
		else
			memset(hx_touch_data->hx_state_info, 0x00, sizeof(hx_touch_data->hx_state_info));

		if ((HX_HW_RESET_ACTIVATE)
#ifdef HX_ESD_RECOVERY
	    || (HX_ESD_RESET_ACTIVATE)
#endif
	   ) {
			memcpy(hx_touch_data->hx_rawdata_buf, &buf[hx_touch_data->touch_info_size], hx_touch_data->touch_all_size - hx_touch_data->touch_info_size);
		}
	} else if (ts_path == HX_REPORT_COORD_RAWDATA) {
		memcpy(hx_touch_data->hx_coord_buf, &buf[0], hx_touch_data->touch_info_size);

		if (buf[hx_state_info_pos] != 0xFF && buf[hx_state_info_pos + 1] != 0xFF)
			memcpy(hx_touch_data->hx_state_info, &buf[hx_state_info_pos], 2);
		else
			memset(hx_touch_data->hx_state_info, 0x00, sizeof(hx_touch_data->hx_state_info));

		memcpy(hx_touch_data->hx_rawdata_buf, &buf[hx_touch_data->touch_info_size], hx_touch_data->touch_all_size - hx_touch_data->touch_info_size);
	}
#if defined(HX_SMART_WAKEUP)
	else if (ts_path == HX_REPORT_SMWP_EVENT)
		memcpy(hx_touch_data->hx_event_buf, buf, hx_touch_data->event_size);
#endif
	else {
		E("%s, Fail Path!\n", __func__);
		ts_status = HX_PATH_FAIL;
	}

	if (g_ts_dbg != 0)
		I("%s: End, ts_status=%d!\n", __func__, ts_status);
	return ts_status;
}
/* end assign_data*/

/* start parse_report_data*/
int himax_parse_report_points(struct himax_ts_data *ts, int ts_path, int ts_status)
{
	int x = 0;
	int y = 0;
	int w = 0;
	int base = 0;
	int32_t	loop_i = 0;

	if (g_ts_dbg != 0)
		I("%s: start!\n", __func__);

	ts->old_finger = ts->pre_finger_mask;
	ts->pre_finger_mask = 0;
	hx_touch_data->finger_num = hx_touch_data->hx_coord_buf[ts->coordInfoSize - 4] & 0x0F;
	hx_touch_data->finger_on = 1;
	AA_press = 1;

	g_target_report_data->finger_num = hx_touch_data->finger_num;
	g_target_report_data->finger_on = hx_touch_data->finger_on;

	if (g_ts_dbg != 0)
		I("%s:finger_num = 0x%2X, finger_on = %d\n", __func__, g_target_report_data->finger_num, g_target_report_data->finger_on);

	for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
		base = loop_i * 4;
		x = hx_touch_data->hx_coord_buf[base] << 8 | hx_touch_data->hx_coord_buf[base + 1];
		y = (hx_touch_data->hx_coord_buf[base + 2] << 8 | hx_touch_data->hx_coord_buf[base + 3]);
		w = hx_touch_data->hx_coord_buf[(ts->nFinger_support * 4) + loop_i];

		if (g_ts_dbg != 0)
			D("%s: now parsing[%d]:x=%d, y=%d, w=%d\n", __func__, loop_i, x, y, w);

		if (x >= 0 && x <= ts->pdata->abs_x_max && y >= 0 && y <= ts->pdata->abs_y_max) {
			hx_touch_data->finger_num--;

			g_target_report_data->x[loop_i] = x;
			g_target_report_data->y[loop_i] = y;
			g_target_report_data->w[loop_i] = w;
			g_target_report_data->finger_id[loop_i] = 1;

			/* I("%s: g_target_report_data->x[loop_i]=%d, g_target_report_data->y[loop_i]=%d, g_target_report_data->w[loop_i]=%d", */
			/*	__func__, g_target_report_data->x[loop_i], g_target_report_data->y[loop_i], g_target_report_data->w[loop_i]); */


			if (!ts->first_pressed) {
				ts->first_pressed = 1;
				D("S1@%d, %d\n", x, y);
			}

			ts->pre_finger_data[loop_i][0] = x;
			ts->pre_finger_data[loop_i][1] = y;

			ts->pre_finger_mask = ts->pre_finger_mask + (1 << loop_i);
		} else {/* report coordinates */
			g_target_report_data->x[loop_i] = x;
			g_target_report_data->y[loop_i] = y;
			g_target_report_data->w[loop_i] = w;
			g_target_report_data->finger_id[loop_i] = 0;

			if (loop_i == 0 && ts->first_pressed == 1) {
				ts->first_pressed = 2;
				D("E1@%d, %d\n", ts->pre_finger_data[0][0],
					ts->pre_finger_data[0][1]);
			}
		}
	}

	if (g_ts_dbg != 0) {
		for (loop_i = 0; loop_i < 10; loop_i++)
			D("DBG X=%d  Y=%d ID=%d\n", g_target_report_data->x[loop_i], g_target_report_data->y[loop_i], g_target_report_data->finger_id[loop_i]);

		D("DBG finger number %d\n", g_target_report_data->finger_num);
	}

	if (g_ts_dbg != 0)
		I("%s: end!\n", __func__);
	return ts_status;
}

static int himax_parse_report_data(struct himax_ts_data *ts, int ts_path, int ts_status)
{

	if (g_ts_dbg != 0)
		I("%s: start now_status=%d!\n", __func__, ts_status);


	EN_NoiseFilter = (hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT + 2] >> 3);
	/* I("EN_NoiseFilter=%d\n", EN_NoiseFilter); */
	EN_NoiseFilter = EN_NoiseFilter & 0x01;
	/* I("EN_NoiseFilter2=%d\n", EN_NoiseFilter); */
#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
	tpd_key = (hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT + 2] >> 4);

	/* All (VK+AA)leave */
	if (tpd_key == 0x0F)
		tpd_key = 0x00;
#endif
	p_point_num = ts->hx_point_num;

	switch (ts_path) {
	case HX_REPORT_COORD:
		ts_status = himax_parse_report_points(ts, ts_path, ts_status);
		break;
	case HX_REPORT_COORD_RAWDATA:
		/* touch monitor rawdata */
		if (debug_data != NULL) {
			if (debug_data->fp_set_diag_cmd(ic_data, hx_touch_data))
				I("%s: coordinate dump fail and bypass with checksum err\n", __func__);
		} else
			E("%s,There is no init set_diag_cmd\n", __func__);

		ts_status = himax_parse_report_points(ts, ts_path, ts_status);
		break;
#ifdef HX_SMART_WAKEUP
	case HX_REPORT_SMWP_EVENT:
		himax_wake_event_parse(ts, ts_status);
		break;
#endif
	default:
		E("%s:Fail Path!\n", __func__);
		ts_status = HX_PATH_FAIL;
		break;
	}
	if (g_ts_dbg != 0)
		I("%s: end now_status=%d!\n", __func__, ts_status);
	return ts_status;
}

/* end parse_report_data */

static void himax_report_all_leave_event(struct himax_ts_data *ts)
{
	int loop_i = 0;

	for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
#ifndef	HX_PROTOCOL_A
		input_mt_slot(ts->input_dev, loop_i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
#endif
	}
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
	input_sync(ts->input_dev);
}

/* start report_data */
#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
static void himax_key_report_operation(int tp_key_index, struct himax_ts_data *ts)
{
	uint16_t x_position = 0, y_position = 0;

	if (g_ts_dbg != 0)
		I("%s: Entering\n", __func__);

	if (tp_key_index != 0x00) {
		I("virtual key index =%x\n", tp_key_index);

		if (tp_key_index == 0x01) {
			vk_press = 1;
			I("back key pressed\n");

			if (ts->pdata->virtual_key) {
				if (ts->button[0].index) {
					x_position = (ts->button[0].x_range_min + ts->button[0].x_range_max) / 2;
					y_position = (ts->button[0].y_range_min + ts->button[0].y_range_max) / 2;
				}

#ifdef	HX_PROTOCOL_A
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y_position);
				input_mt_sync(ts->input_dev);
#else
				input_mt_slot(ts->input_dev, 0);
				input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y_position);
#endif
			} else {
				input_report_key(ts->input_dev, KEY_BACK, 1);
			}
		} else if (tp_key_index == 0x02) {
			vk_press = 1;
			I("home key pressed\n");

			if (ts->pdata->virtual_key) {
				if (ts->button[1].index) {
					x_position = (ts->button[1].x_range_min + ts->button[1].x_range_max) / 2;
					y_position = (ts->button[1].y_range_min + ts->button[1].y_range_max) / 2;
				}

#ifdef	HX_PROTOCOL_A
				input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y_position);
				input_mt_sync(ts->input_dev);
#else
				input_mt_slot(ts->input_dev, 0);
				input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y_position);
#endif
			} else
				input_report_key(ts->input_dev, KEY_HOME, 1);

		} else if (tp_key_index == 0x04) {
			vk_press = 1;
			I("APP_switch key pressed\n");

			if (ts->pdata->virtual_key) {
				if (ts->button[2].index) {
					x_position = (ts->button[2].x_range_min + ts->button[2].x_range_max) / 2;
					y_position = (ts->button[2].y_range_min + ts->button[2].y_range_max) / 2;
				}

#ifdef HX_PROTOCOL_A
				input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y_position);
				input_mt_sync(ts->input_dev);
#else
				input_mt_slot(ts->input_dev, 0);
				input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y_position);
#endif
			} else
				input_report_key(ts->input_dev, KEY_APPSELECT, 1);
		}

		input_sync(ts->input_dev);
	} else { /* tp_key_index =0x00 */
		I("virtual key released\n");
		vk_press = 0;
#ifndef	HX_PROTOCOL_A
		input_mt_slot(ts->input_dev, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
#else
		input_mt_sync(ts->input_dev);
#endif
		input_report_key(ts->input_dev, KEY_BACK, 0);
		input_report_key(ts->input_dev, KEY_HOME, 0);
		input_report_key(ts->input_dev, KEY_APPSELECT, 0);
#ifndef	HX_PROTOCOL_A
		input_sync(ts->input_dev);
#endif
	}
}

void himax_finger_report_key(struct himax_ts_data *ts)
{
	if (hx_point_num != 0) {
		/* Touch KEY */
		if ((tpd_key_old != 0x00) && (tpd_key == 0x00)) {
			/* temp_x[0] = 0xFFFF; */
			/* temp_y[0] = 0xFFFF; */
			/* temp_x[1] = 0xFFFF; */
			/* temp_y[1] = 0xFFFF; */
			hx_touch_data->finger_on = 0;
#ifdef HX_PROTOCOL_A
			input_report_key(ts->input_dev, BTN_TOUCH, hx_touch_data->finger_on);
#endif
			himax_key_report_operation(tpd_key, ts);
		}

#ifndef HX_PROTOCOL_A
		input_report_key(ts->input_dev, BTN_TOUCH, hx_touch_data->finger_on);
#endif
		input_sync(ts->input_dev);
	}
}

void himax_finger_leave_key(struct himax_ts_data *ts)
{
	if (tpd_key != 0x00) {
		hx_touch_data->finger_on = 1;
#ifdef HX_PROTOCOL_A
		input_report_key(ts->input_dev, BTN_TOUCH, hx_touch_data->finger_on);
#endif
		himax_key_report_operation(tpd_key, ts);
	} else if ((tpd_key_old != 0x00) && (tpd_key == 0x00)) {
		hx_touch_data->finger_on = 0;
#ifdef HX_PROTOCOL_A
		input_report_key(ts->input_dev, BTN_TOUCH, hx_touch_data->finger_on);
#endif
		himax_key_report_operation(tpd_key, ts);
	}

#ifndef HX_PROTOCOL_A
	input_report_key(ts->input_dev, BTN_TOUCH, hx_touch_data->finger_on);
#endif
	input_sync(ts->input_dev);
}

static void himax_report_key(struct himax_ts_data *ts)
{
	if (hx_point_num != 0)
		himax_finger_report_key(ts); /* Touch KEY */
	else
		himax_finger_leave_key(ts); /* Key */

	tpd_key_old = tpd_key;
	Last_EN_NoiseFilter = EN_NoiseFilter;
}
#endif

/* start report_point*/
static void himax_finger_report(struct himax_ts_data *ts)
{
	int i = 0;
	bool valid = false;

	if (g_ts_dbg != 0) {
		I("%s:start\n", __func__);
		I("hx_touch_data->finger_num=%d\n", hx_touch_data->finger_num);
	}
	for (i = 0; i < ts->nFinger_support; i++) {
		if (g_target_report_data->x[i] >= 0 && g_target_report_data->x[i] <= ts->pdata->abs_x_max
			&& g_target_report_data->y[i] >= 0 && g_target_report_data->y[i] <= ts->pdata->abs_y_max)
			valid = true;
		else
			valid = false;
		if (g_ts_dbg != 0)
			I("valid=%d\n", valid);
		if (valid) {
			if (g_ts_dbg != 0)
				I("g_target_report_data->x[i]=%d, g_target_report_data->y[i]=%d, g_target_report_data->w[i]=%d\n",
					g_target_report_data->x[i], g_target_report_data->y[i], g_target_report_data->w[i]);
#ifndef	HX_PROTOCOL_A
			input_mt_slot(ts->input_dev, i);
			ts->last_slot = i;
			input_mt_report_slot_state(ts->input_dev,
							MT_TOOL_FINGER, 1);
#endif
			input_report_key(ts->input_dev, BTN_TOUCH, g_target_report_data->finger_on);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_target_report_data->w[i]);
#ifndef	HX_PROTOCOL_A
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, g_target_report_data->w[i]);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, g_target_report_data->w[i]);
#endif
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, g_target_report_data->x[i]);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, g_target_report_data->y[i]);
#ifdef	HX_PROTOCOL_A
			input_mt_sync(ts->input_dev);
#endif

		} else {
			input_mt_slot(ts->input_dev, i);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
		}
	}

	input_report_key(ts->input_dev, BTN_TOUCH, g_target_report_data->finger_on);
	input_sync(ts->input_dev);

	if (g_ts_dbg != 0)
		I("%s:end\n", __func__);
}

static void himax_finger_leave(struct himax_ts_data *ts)
{
	int32_t	loop_i = 0;

	if (g_ts_dbg != 0)
		I("%s: start!\n", __func__);
#if defined(HX_PALM_REPORT)
	if (himax_palm_detect(hx_touch_data->hx_coord_buf) == NO_ERR) {
		I(" %s HX_PALM_REPORT KEY power event press\n", __func__);
		input_report_key(ts->input_dev, KEY_POWER, 1);
		input_sync(ts->input_dev);
		msleep(100);

		I(" %s HX_PALM_REPORT KEY power event release\n", __func__);
		input_report_key(ts->input_dev, KEY_POWER, 0);
		input_sync(ts->input_dev);
		return;
	}
#endif

	hx_touch_data->finger_on = 0;
	AA_press = 0;

#ifdef HX_PROTOCOL_A
	input_mt_sync(ts->input_dev);
#endif

	for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
		if (((ts->old_finger >> loop_i) & 1) == 1) {
			input_mt_slot(ts->input_dev, loop_i);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
		}
	}

	if (ts->pre_finger_mask > 0)
		ts->pre_finger_mask = 0;

	if (ts->first_pressed == 1) {
		ts->first_pressed = 2;
		I("E1@%d, %d\n", ts->pre_finger_data[0][0], ts->pre_finger_data[0][1]);
	}

	/* if (ts->debug_log_level & BIT(1)) */
	/*	himax_log_touch_event(x, y, w, loop_i, EN_NoiseFilter, HX_FINGER_LEAVE); */

	input_report_key(ts->input_dev, BTN_TOUCH, hx_touch_data->finger_on);
	input_sync(ts->input_dev);


	if (g_ts_dbg != 0)
		I("%s: end!\n", __func__);
}

static void himax_report_points(struct himax_ts_data *ts)
{
	if (g_ts_dbg != 0)
		I("%s: start!\n", __func__);

	if (ts->hx_point_num != 0)
		himax_finger_report(ts);
	else
		himax_finger_leave(ts);


	Last_EN_NoiseFilter = EN_NoiseFilter;

	if (g_ts_dbg != 0)
		I("%s: end!\n", __func__);
}
/* end report_points */

int himax_report_data(struct himax_ts_data *ts, int ts_path, int ts_status)
{
	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	if (ts_path == HX_REPORT_COORD || ts_path == HX_REPORT_COORD_RAWDATA) {
		if (hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT] == 0xff)
			ts->hx_point_num = 0;
		else
			ts->hx_point_num = hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT] & 0x0f;

		/* Touch Point information */
		himax_report_points(ts);

#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
		/* report key(question mark) */
		if (tpd_key && tpd_key_old)
			himax_report_key(ts);
#endif
	}

#ifdef HX_SMART_WAKEUP
	else if (ts_path == HX_REPORT_SMWP_EVENT) {
		__pm_wakeup_event(&ts->ts_SMWP_wake_src, TS_WAKE_LOCK_TIMEOUT);
		himax_wake_event_report();
	}
#endif

	else {
		E("%s:Fail Path!\n", __func__);
		ts_status = HX_PATH_FAIL;
	}

	if (g_ts_dbg != 0)
		I("%s: END, ts_status=%d!\n", __func__, ts_status);
	return ts_status;
}
/* end report_data */

static int himax_ts_operation(struct himax_ts_data *ts, int ts_path, int ts_status)
{
	uint8_t hw_reset_check[2];
	uint8_t buf[HX_REPORT_SZ];

	memset(buf, 0x00, sizeof(buf));
	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));

	ts_status = himax_touch_get(ts, buf, ts_path, ts_status);
	if (ts_status == HX_TS_GET_DATA_FAIL)
		goto END_FUNCTION;

	ts_status = himax_err_ctrl(ts, buf, ts_path, ts_status);
	if (ts_status == HX_REPORT_DATA || ts_status == HX_TS_NORMAL_END) {
		ts_status = himax_distribute_touch_data(buf, ts_path, ts_status);
		ts_status = himax_parse_report_data(ts, ts_path, ts_status);
	} else
		goto END_FUNCTION;

	ts_status = himax_report_data(ts, ts_path, ts_status);


END_FUNCTION:
	return ts_status;
}

void himax_ts_work(struct himax_ts_data *ts)
{

	int ts_status = HX_TS_NORMAL_END;
	int ts_path = 0;

	if (debug_data != NULL)
		debug_data->fp_ts_dbg_func(ts, HX_FINGER_ON);

#if defined(HX_USB_DETECT_GLOBAL)
	himax_cable_detect_func(false);
#endif

	ts_path = himax_ts_work_status(ts);
	switch (ts_path) {
	case HX_REPORT_COORD:
		ts_status = himax_ts_operation(ts, ts_path, ts_status);
		break;
	case HX_REPORT_SMWP_EVENT:
		ts_status = himax_ts_operation(ts, ts_path, ts_status);
		break;
	case HX_REPORT_COORD_RAWDATA:
		ts_status = himax_ts_operation(ts, ts_path, ts_status);
		break;
	default:
		E("%s:Path Fault! value=%d\n", __func__, ts_path);
		goto END_FUNCTION;
	}

	if (ts_status == HX_TS_GET_DATA_FAIL)
		goto GET_TOUCH_FAIL;
	else
		goto END_FUNCTION;

GET_TOUCH_FAIL:
	E("%s: Now reset the Touch chip.\n", __func__);
#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(false, true);
#endif
END_FUNCTION:
	if (debug_data != NULL)
		debug_data->fp_ts_dbg_func(ts, HX_FINGER_LEAVE);


}
/* end ts_work */
enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer)
{
	struct himax_ts_data *ts;

	ts = container_of(timer, struct himax_ts_data, timer);
	queue_work(ts->himax_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

#if defined(HX_USB_DETECT_CALLBACK)
static void himax_cable_tp_status_handler_func(int connect_status)
{
	struct himax_ts_data *ts;

	I("Touch: cable change to %d\n", connect_status);
	ts = private_ts;

	if (ts->cable_config) {
		if (!atomic_read(&ts->suspend_mode)) {
			if ((!!connect_status) != ts->usb_connected) {
				if (!!connect_status) {
					ts->cable_config[1] = 0x01;
					ts->usb_connected = 0x01;
				}

				else {
					ts->cable_config[1] = 0x00;
					ts->usb_connected = 0x00;
				}

				if (himax_bus_master_write(ts->cable_config, sizeof(ts->cable_config), HIMAX_I2C_RETRY_TIMES) < 0) {
					E("%s: i2c access fail: cable_config!\n", __func__);
					return;
				}
				I("%s: Cable status change: 0x%2.2X\n", __func__, ts->cable_config[1]);
			} else
				I("%s: Cable status is the same as previous one, ignore.\n", __func__);

		} else {
			if (connect_status)
				ts->usb_connected = 0x01;
			else
				ts->usb_connected = 0x00;

			I("%s: Cable status remembered: 0x%2.2X\n", __func__, ts->usb_connected);
		}
	}
}

static const struct t_cable_status_notifier himax_cable_status_handler = {
	.name = "usb_tp_connected",
	.func = himax_cable_tp_status_handler_func,
};

#endif

#ifdef HX_AUTO_UPDATE_FW
void himax_update_register(struct work_struct *work)
{
	D(" %s in", __func__);

	if (i_update_FW() <= 0)
		D("FW =NOT UPDATE=\n");
	else
		D("Have new FW =UPDATE=\n");
}
#endif

#ifdef CONFIG_DRM
int himax_fb_register(struct himax_ts_data *ts)
{
	int ret = 0;

	D(" %s in\n", __func__);
	ts->fb_notif.notifier_call = drm_notifier_callback;
	ret = msm_drm_register_client(&ts->fb_notif);
	if (ret)
		E(" Unable to register fb_notifier: %d\n", ret);

	return ret;
}

#elif defined CONFIG_FB
int himax_fb_register(struct himax_ts_data *ts)
{
	int ret = 0;

	D(" %s in\n", __func__);
	ts->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts->fb_notif);

	if (ret)
		E(" Unable to register fb_notifier: %d\n", ret);

	return ret;
}
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
static void himax_ito_test_work(struct work_struct *work)
{
	I(" %s in\n", __func__);
	himax_ito_test();
}
#endif

int himax_chip_common_init(void)
{
#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
	bool auto_update_flag = false;
#endif
	int ret = 0, err = -1;
	struct himax_ts_data *ts = private_ts;
	struct himax_i2c_platform_data *pdata;

	D("PDATA START\n");
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) { /* Allocate Platform data space */
		err = -ENOMEM;
		goto err_dt_platform_data_fail;
	}

	D("ic_data START\n");
	ic_data = kzalloc(sizeof(*ic_data), GFP_KERNEL);
	if (ic_data == NULL) { /* Allocate IC data space */
		err = -ENOMEM;
		goto err_dt_ic_data_fail;
	}

	/* allocate report data */
	hx_touch_data = kzalloc(sizeof(struct himax_report_data), GFP_KERNEL);
	if (hx_touch_data == NULL) {
		err = -ENOMEM;
		goto err_alloc_touch_data_failed;
	}

	if (himax_parse_dt(ts, pdata) < 0) {
		E(" pdata is NULL for DT\n");
		err = -ECANCELED;
		goto err_alloc_dt_pdata_failed;
	}

#ifdef HX_RST_PIN_FUNC
	ts->rst_gpio = pdata->gpio_reset;
#endif
	himax_gpio_power_config(pdata);
#ifndef CONFIG_OF

	if (pdata->power) {
		ret = pdata->power(1);

		if (ret < 0) {
			E("%s: power on failed\n", __func__);
			err = ret;
			goto err_power_failed;
		}
	}

#endif

	if (g_core_fp.fp_chip_detect != NULL && g_core_fp.fp_chip_init != NULL) {
		if (g_core_fp.fp_chip_detect() != false) {
			g_core_fp.fp_chip_init();
		} else {
			E("%s: chip detect failed!\n", __func__);
			err = -ECANCELED;
			goto error_ic_detect_failed;
		}
	} else {
		E("%s: function point is NULL!\n", __func__);
		err = -ECANCELED;
		goto error_ic_detect_failed;
	}

	if (pdata->virtual_key)
		ts->button = pdata->virtual_key;

	g_core_fp.fp_read_FW_ver();

#ifdef HX_AUTO_UPDATE_FW
	queue_delayed_work(ts->himax_update_wq, &ts->work_update,
		msecs_to_jiffies(2000));
#endif
#ifdef HX_ZERO_FLASH
	auto_update_flag = true;
	ts->himax_0f_update_wq = create_singlethread_workqueue("HMX_0f_update_request");
	INIT_DELAYED_WORK(&ts->work_0f_update, g_core_fp.fp_0f_operation);
	queue_delayed_work(ts->himax_0f_update_wq, &ts->work_0f_update, msecs_to_jiffies(2000));
#endif

	/* Himax Power On and Load Config */
	if (himax_loadSensorConfig(pdata)) {
		E("%s: Load Sesnsor configuration failed, unload driver.\n", __func__);
		err = -ECANCELED;
		goto err_detect_failed;
	}

	g_core_fp.fp_power_on_init();
	calculate_point_number();

#ifdef CONFIG_OF
	ts->power = pdata->power;
#endif
	ts->pdata = pdata;
	ts->x_channel = ic_data->HX_RX_NUM;
	ts->y_channel = ic_data->HX_TX_NUM;
	ts->nFinger_support = ic_data->HX_MAX_PT;
	/* calculate the i2c data size */
	calcDataSize(ts->nFinger_support);
	D("%s: calcDataSize complete\n", __func__);
#ifdef CONFIG_OF
	ts->pdata->abs_pressure_min        = 0;
	ts->pdata->abs_pressure_max        = 200;
	ts->pdata->abs_width_min           = 0;
	ts->pdata->abs_width_max           = 200;
	pdata->cable_config[0]             = 0xF0;
	pdata->cable_config[1]             = 0x00;
#endif
	ts->suspended                      = false;
#if defined(HX_USB_DETECT_CALLBACK) || defined(HX_USB_DETECT_GLOBAL)
	ts->usb_connected = 0x00;
	ts->cable_config = pdata->cable_config;
#endif
#ifdef	HX_PROTOCOL_A
	ts->protocol_type = PROTOCOL_TYPE_A;
#else
	ts->protocol_type = PROTOCOL_TYPE_B;
#endif
	D("%s: Use Protocol Type %c\n", __func__,
	  ts->protocol_type == PROTOCOL_TYPE_A ? 'A' : 'B');

	ret = himax_input_register(ts);
	if (ret) {
		E("%s: Unable to register %s input device\n",
		  __func__, ts->input_dev->name);
		err = ret;
		goto err_input_register_device_failed;
	}

#ifdef HX_SMART_WAKEUP
	ts->SMWP_enable = 0;
	wakeup_source_init(&ts->ts_SMWP_wake_src, WAKE_LOCK_SUSPEND, HIMAX_common_NAME);
#endif
#ifdef HX_HIGH_SENSE
	ts->HSEN_enable = 0;
#endif
#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
	ts->ito_test_wq = create_singlethread_workqueue("himax_ito_test_wq");

	if (!ts->ito_test_wq) {
		E("%s: ito test workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_ito_test_wq_failed;
	}

	INIT_WORK(&ts->ito_test_work, himax_ito_test_work);
#endif

	/* touch data init */
	err = himax_report_data_init();
	if (err)
		goto err_report_data_init_failed;

	if (himax_common_proc_init()) {
		E(" %s: himax_common proc_init failed!\n", __func__);
		err = -ECANCELED;
		goto err_creat_proc_file_failed;
	}

#if defined(HX_USB_DETECT_CALLBACK)

	if (ts->cable_config)
		cable_detect_register_notifier(&himax_cable_status_handler);

#endif
	err = himax_ts_register_interrupt();

	if (err)
		goto err_register_interrupt_failed;


#ifdef CONFIG_TOUCHSCREEN_HIMAX_DEBUG
	if (himax_debug_init())
		E(" %s: debug initial failed!\n", __func__);
#endif


	return 0;
err_register_interrupt_failed:
remove_proc_entry(HIMAX_PROC_TOUCH_FOLDER, NULL);
err_creat_proc_file_failed:
err_report_data_init_failed:
#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
	destroy_workqueue(ts->ito_test_wq);
err_ito_test_wq_failed:
#endif
#ifdef HX_SMART_WAKEUP
	wakeup_source_trash(&ts->ts_SMWP_wake_src);
#endif
err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_detect_failed:
#ifdef HX_AUTO_UPDATE_FW
	if (auto_update_flag) {
		cancel_delayed_work_sync(&ts->work_update);
		destroy_workqueue(ts->himax_update_wq);
	}

#endif

error_ic_detect_failed:
	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);

#ifdef HX_RST_PIN_FUNC

	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);

#endif

#ifndef CONFIG_OF
err_power_failed:
#endif

err_alloc_dt_pdata_failed:
	kfree(hx_touch_data);
err_alloc_touch_data_failed:
	kfree(ic_data);
err_dt_ic_data_fail:
	kfree(pdata);
err_dt_platform_data_fail:
	probe_fail_flag = 1;
	return err;
}

void himax_chip_common_deinit(void)
{
	struct himax_ts_data *ts = private_ts;

#ifdef CONFIG_TOUCHSCREEN_HIMAX_DEBUG
	himax_debug_remove();
#endif

	remove_proc_entry(HIMAX_PROC_TOUCH_FOLDER, NULL);
	himax_common_proc_deinit();

	if (!ts->use_irq) {
		hrtimer_cancel(&ts->timer);
		destroy_workqueue(ts->himax_wq);
	}

#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
	destroy_workqueue(ts->ito_test_wq);
#endif
#ifdef HX_SMART_WAKEUP
	wakeup_source_trash(&ts->ts_SMWP_wake_src);
#endif

#ifdef CONFIG_DRM
	if (msm_drm_unregister_client(&ts->fb_notif))
		E("Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_FB)
	if (fb_unregister_client(&ts->fb_notif))
		E("Error occurred while unregistering fb_notifier.\n");
#endif
	input_free_device(ts->input_dev);
#ifdef HX_ZERO_FLASH
	cancel_delayed_work_sync(&ts->work_0f_update);
	destroy_workqueue(ts->himax_0f_update_wq);
#endif
#ifdef HX_AUTO_UPDATE_FW
	cancel_delayed_work_sync(&ts->work_update);
	destroy_workqueue(ts->himax_update_wq);
#endif
	if (gpio_is_valid(ts->pdata->gpio_irq))
		gpio_free(ts->pdata->gpio_irq);
#ifdef HX_RST_PIN_FUNC
	if (gpio_is_valid(ts->pdata->gpio_reset))
		gpio_free(ts->pdata->gpio_reset);
#endif

	kfree(hx_touch_data);
	kfree(ic_data);
	kfree(ts->pdata);
	kfree(ts->i2c_data);
	kfree(ts);
	probe_fail_flag = 0;
}

int himax_chip_common_suspend(struct himax_ts_data *ts)
{
	int ret;

	if (ts->suspended) {
		I("%s: Already suspended. Skipped.\n", __func__);
		return 0;
	}

	ts->suspended = true;
	D("%s: enter\n", __func__);

	if (debug_data != NULL && debug_data->flash_dump_going == true) {
		I("[himax] %s: Flash dump is going, reject suspend\n", __func__);
		return 0;
	}

#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) || defined(HX_USB_DETECT_GLOBAL)
#ifndef HX_RESUME_SEND_CMD
	g_core_fp.fp_resend_cmd_func(ts->suspended);
#endif
#endif
#ifdef HX_SMART_WAKEUP

	if (ts->SMWP_enable) {
		atomic_set(&ts->suspend_mode, 1);
		ts->pre_finger_mask = 0;
		FAKE_POWER_KEY_SEND = false;
		I("[himax] %s: SMART_WAKEUP enable, reject suspend\n", __func__);
		return 0;
	}

#endif
	himax_int_enable(0);
	g_core_fp.fp_suspend_ic_action();

	if (!ts->use_irq) {
		ret = cancel_work_sync(&ts->work);

		if (ret)
			himax_int_enable(1);
	}

	/* ts->first_pressed = 0; */
	atomic_set(&ts->suspend_mode, 1);
	ts->pre_finger_mask = 0;

	if (ts->pdata->powerOff3V3 && ts->pdata->power)
		ts->pdata->power(0);

	D("%s: END\n", __func__);
	return 0;
}

int himax_chip_common_resume(struct himax_ts_data *ts)
{
	D("%s: enter\n", __func__);

	if (ts->suspended == false) {
		D("%s: It had entered resume, skip this step\n", __func__);
		return 0;
	}
	ts->suspended = false;

	atomic_set(&ts->suspend_mode, 0);

	if (ts->pdata->powerOff3V3 && ts->pdata->power)
		ts->pdata->power(1);

#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) || defined(HX_USB_DETECT_GLOBAL)
	g_core_fp.fp_resend_cmd_func(ts->suspended);
#elif defined(HX_RESUME_HW_RESET)
	g_core_fp.fp_ic_reset(false, false);
#endif
	himax_report_all_leave_event(ts);

	g_core_fp.fp_resume_ic_action();
	himax_int_enable(1);

	D("%s: END\n", __func__);
	return 0;
}
