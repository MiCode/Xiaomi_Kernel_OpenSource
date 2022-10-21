/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for common functions
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_inspection.h"
#include "himax_modular.h"

#if defined(HX_CONFIG_DRM)
#include <drm/drm_notifier_mi.h>
#endif

#if defined(HX_PEN_DETECT_GLOBAL)
extern int pen_charge_state_notifier_register_client(struct notifier_block *nb);
extern int pen_charge_state_notifier_unregister_client(struct notifier_block *nb);
#endif

#if defined(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE)
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
#endif

#if defined(HX_PEN_CONNECT_STRATEGY)
struct class *sys_class;
extern void class_destroy(struct class *cls);
#define HIMAX_SYS_NODE "himax"
#endif

#if defined(__HIMAX_MOD__)
int (*hx_msm_drm_register_client)(struct notifier_block *nb);
int (*hx_msm_drm_unregister_client)(struct notifier_block *nb);
#endif

#if defined(HX_SMART_WAKEUP)
#define GEST_SUP_NUM 26
uint8_t gest_event[GEST_SUP_NUM] = {
	0x80, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x81, 0x1D, 0x2D, 0x3D, 0x1F, 0x2F, 0x51, 0x52,
	0x53, 0x54};

uint16_t gest_key_def[GEST_SUP_NUM] = {
	HX_KEY_DOUBLE_CLICK, HX_KEY_UP, HX_KEY_DOWN, HX_KEY_LEFT,
	HX_KEY_RIGHT,	HX_KEY_C, HX_KEY_Z, HX_KEY_M,
	HX_KEY_O, HX_KEY_S, HX_KEY_V, HX_KEY_W,
	HX_KEY_E, HX_KEY_LC_M, HX_KEY_AT, HX_KEY_RESERVE,
	HX_KEY_FINGER_GEST,	HX_KEY_V_DOWN, HX_KEY_V_LEFT, HX_KEY_V_RIGHT,
	HX_KEY_F_RIGHT,	HX_KEY_F_LEFT, HX_KEY_DF_UP, HX_KEY_DF_DOWN,
	HX_KEY_DF_LEFT,	HX_KEY_DF_RIGHT};

uint8_t *wake_event_buffer;
#endif

#define SUPPORT_FINGER_DATA_CHECKSUM 0x0F
#define TS_WAKE_LOCK_TIMEOUT		(5000)
#define FRAME_COUNT 5

#if defined(HX_TP_PROC_GUEST_INFO)
struct hx_guest_info *g_guest_info_data;
EXPORT_SYMBOL(g_guest_info_data);

char *g_guest_info_item[] = {
	"projectID",
	"CGColor",
	"BarCode",
	"Reserve1",
	"Reserve2",
	"Reserve3",
	"Reserve4",
	"Reserve5",
	"VCOM",
	"Vcom-3Gar",
	NULL
};
#endif

uint32_t g_hx_chip_inited;

#if defined(__EMBEDDED_FW__)
struct firmware g_embedded_fw = {
	.data = _binary___Himax_firmware_bin_start,
};
#endif

#if defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH)
bool g_boot_upgrade_flag;
const struct firmware *hxfw;
int g_i_FW_VER;
int g_i_CFG_VER;
int g_i_CID_MAJ;
int g_i_CID_MIN;
#if defined(HX_ZERO_FLASH)
int g_f_0f_updat;
#endif
#ifdef HX_PARSE_FROM_DT
char *g_fw_boot_upgrade_name;
EXPORT_SYMBOL(g_fw_boot_upgrade_name);
#if defined(HX_ZERO_FLASH)
char *g_fw_mp_upgrade_name;
EXPORT_SYMBOL(g_fw_mp_upgrade_name);
#endif
#else
char *g_fw_boot_upgrade_name = BOOT_UPGRADE_FWNAME;
EXPORT_SYMBOL(g_fw_boot_upgrade_name);
#if defined(HX_ZERO_FLASH)
char *g_fw_mp_upgrade_name = MPAP_FWNAME;
EXPORT_SYMBOL(g_fw_mp_upgrade_name);
#endif
#endif
#endif

#ifdef HX_PARSE_FROM_DT
uint32_t g_proj_id = 0xffff;
EXPORT_SYMBOL(g_proj_id);
#endif

struct himax_ts_data *private_ts;
EXPORT_SYMBOL(private_ts);

struct himax_ic_data *ic_data;
EXPORT_SYMBOL(ic_data);

struct himax_report_data *hx_touch_data;
EXPORT_SYMBOL(hx_touch_data);

struct himax_core_fp g_core_fp;
EXPORT_SYMBOL(g_core_fp);

struct himax_debug *debug_data;
EXPORT_SYMBOL(debug_data);

struct proc_dir_entry *himax_touch_proc_dir;
EXPORT_SYMBOL(himax_touch_proc_dir);

int g_mmi_refcnt;
EXPORT_SYMBOL(g_mmi_refcnt);

#define HIMAX_PROC_TOUCH_FOLDER "android_touch"
struct himax_target_report_data *g_target_report_data;
EXPORT_SYMBOL(g_target_report_data);

static void himax_report_all_leave_event(struct himax_ts_data *ts);

struct filename* (*kp_getname_kernel)(const char *filename);
void (*kp_putname_kernel)(struct filename *name);
struct file* (*kp_file_open_name)(struct filename *name,
		int flags, umode_t mode);

unsigned long FW_VER_MAJ_FLASH_ADDR;
EXPORT_SYMBOL(FW_VER_MAJ_FLASH_ADDR);

unsigned long FW_VER_MIN_FLASH_ADDR;
EXPORT_SYMBOL(FW_VER_MIN_FLASH_ADDR);

unsigned long CFG_VER_MAJ_FLASH_ADDR;
EXPORT_SYMBOL(CFG_VER_MAJ_FLASH_ADDR);

unsigned long CFG_VER_MIN_FLASH_ADDR;
EXPORT_SYMBOL(CFG_VER_MIN_FLASH_ADDR);

unsigned long CID_VER_MAJ_FLASH_ADDR;
EXPORT_SYMBOL(CID_VER_MAJ_FLASH_ADDR);

unsigned long CID_VER_MIN_FLASH_ADDR;
EXPORT_SYMBOL(CID_VER_MIN_FLASH_ADDR);
uint32_t CFG_TABLE_FLASH_ADDR;
EXPORT_SYMBOL(CFG_TABLE_FLASH_ADDR);

uint32_t CFG_TABLE_FLASH_ADDR_T;
EXPORT_SYMBOL(CFG_TABLE_FLASH_ADDR_T);

unsigned char IC_CHECKSUM;
EXPORT_SYMBOL(IC_CHECKSUM);

#if defined(HX_EXCP_RECOVERY)
u8 HX_EXCP_RESET_ACTIVATE;
EXPORT_SYMBOL(HX_EXCP_RESET_ACTIVATE);

int hx_EB_event_flag;
EXPORT_SYMBOL(hx_EB_event_flag);

int hx_EC_event_flag;
EXPORT_SYMBOL(hx_EC_event_flag);

#if defined(HW_ED_EXCP_EVENT)
int hx_EE_event_flag;
EXPORT_SYMBOL(hx_EE_event_flag);
#else
int hx_ED_event_flag;
EXPORT_SYMBOL(hx_ED_event_flag);
#endif

int g_zero_event_count;

#endif

static bool chip_test_r_flag;
u8 HX_HW_RESET_ACTIVATE;

static uint8_t AA_press;
static uint8_t EN_NoiseFilter;
static uint8_t Last_EN_NoiseFilter;

static int p_point_num = 0xFFFF;
static uint8_t p_stylus_num = 0xFF;
static int probe_fail_flag;
#if defined(HX_USB_DETECT_GLOBAL)
bool USB_detect_flag;
#endif

#if defined(HX_GESTURE_TRACK)
static int gest_pt_cnt;
static int gest_pt_x[GEST_PT_MAX_NUM];
static int gest_pt_y[GEST_PT_MAX_NUM];
static int gest_start_x, gest_start_y, gest_end_x, gest_end_y;
static int gest_width, gest_height, gest_mid_x, gest_mid_y;
static int hx_gesture_coor[16];
#endif

int g_ts_dbg;
EXPORT_SYMBOL(g_ts_dbg);

#define HIMAX_PROC_SELF_TEST_FILE	"self_test"
struct proc_dir_entry *himax_proc_self_test_file;

#define XIAOMI_PROC_SELF_TEST_FILE	"tp_selftest"
struct proc_dir_entry *xiaomi_proc_self_test_file;

uint8_t HX_PROC_SEND_FLAG;
EXPORT_SYMBOL(HX_PROC_SEND_FLAG);

#if defined(HX_SMART_WAKEUP)
#define HIMAX_PROC_SMWP_FILE "SMWP"
struct proc_dir_entry *himax_proc_SMWP_file;
#define HIMAX_PROC_GESTURE_FILE "GESTURE"
struct proc_dir_entry *himax_proc_GESTURE_file;
uint8_t HX_SMWP_EN;
#if defined(HX_ULTRA_LOW_POWER)
#define HIMAX_PROC_PSENSOR_FILE "Psensor"
struct proc_dir_entry *himax_proc_psensor_file;
#endif
#endif

#if defined(HX_HIGH_SENSE)
#define HIMAX_PROC_HSEN_FILE "HSEN"
struct proc_dir_entry *himax_proc_HSEN_file;
#endif

#define HIMAX_PROC_VENDOR_FILE "vendor"
struct proc_dir_entry *himax_proc_vendor_file;

#if defined(XM_GAME_MODE)
#define HIMAX_PROC_GAMEMODE_FILE "gamemode"
struct proc_dir_entry *himax_proc_gamemode_file;
#endif

#if defined(HX_PEN_SWITCH)
#define HIMAX_PROC_PSWITCH_FILE "pen_switch"
struct proc_dir_entry *himax_proc_PSWITCH_file;
#endif

extern int himax_lockdown_info_register_read(uint32_t start_addr,uint8_t *flash_tmp_buffer,uint32_t len);
uint8_t bTouchIsAwake = 0;


static ssize_t himax_self_test(struct seq_file *s, void *v)
{
	int val = 0x00;
	size_t ret = 0;

	if (private_ts->suspended == 1) {
		return HX_INIT_FAIL;
	}

	if (private_ts->in_self_test == 1) {
		return ret;
	}
	private_ts->in_self_test = 1;

	himax_int_enable(0);

	val = g_core_fp.fp_chip_self_test(s, v);

#if defined(HX_EXCP_RECOVERY)
	HX_EXCP_RESET_ACTIVATE = 1;
#endif
	himax_int_enable(1);

	private_ts->in_self_test = 0;

	return ret;
}

static void *himax_self_test_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= 1)
		return NULL;


	return (void *)((unsigned long) *pos + 1);
}

static void *himax_self_test_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return NULL;
}

static void himax_self_test_seq_stop(struct seq_file *s, void *v)
{
}

static int himax_self_test_seq_read(struct seq_file *s, void *v)
{
	size_t ret = 0;

	if (chip_test_r_flag) {
#if defined(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)
		if (g_rslt_data)
			seq_printf(s, "%s", g_rslt_data);
		else
#endif
			seq_puts(s, "No chip test data.\n");
	} else {
		himax_self_test(s, v);
	}

	return ret;
}

static const struct seq_operations himax_self_test_seq_ops = {
	.start	= himax_self_test_seq_start,
	.next	= himax_self_test_seq_next,
	.stop	= himax_self_test_seq_stop,
	.show	= himax_self_test_seq_read,
};

static int himax_self_test_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &himax_self_test_seq_ops);
};

static ssize_t himax_self_test_write(struct file *filp, const char __user *buff,
			size_t len, loff_t *data)
{
	char buf[80];

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == 'r') {
		chip_test_r_flag = true;
		I("%s: Start to read chip test data.\n", __func__);
	}	else {
		chip_test_r_flag = false;
		I("%s: Back to do self test.\n", __func__);
	}

	return len;
}

static const struct file_operations himax_proc_self_test_ops = {
	.owner = THIS_MODULE,
	.open = himax_self_test_proc_open,
	.read = seq_read,
	.write = himax_self_test_write,
	.release = seq_release,
};

#if defined(HX_HIGH_SENSE)
static ssize_t himax_HSEN_read(struct file *file, char *buf,
		size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;
	char *temp_buf = NULL;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kcalloc(len, sizeof(char), GFP_KERNEL);
		if (temp_buf != NULL) {
			count = snprintf(temp_buf, PAGE_SIZE, "%d\n",
					ts->HSEN_enable);

			if (copy_to_user(buf, temp_buf, len))
				I("%s, here:%d\n", __func__, __LINE__);

			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
		} else {
			E("%s, Failed to allocate memory\n", __func__);
		}
	} else {
		HX_PROC_SEND_FLAG = 0;
	}

	return count;
}

static ssize_t himax_HSEN_write(struct file *file, const char *buff,
		size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf[80] = {0};

	if (len >= 80) {
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
	I("%s: HSEN_enable = %d.\n", __func__, ts->HSEN_enable);
	return len;
}

static const struct file_operations himax_proc_HSEN_ops = {
	.owner = THIS_MODULE,
	.read = himax_HSEN_read,
	.write = himax_HSEN_write,
};
#endif

#if defined(HX_SMART_WAKEUP)
static ssize_t himax_SMWP_read(struct file *file, char *buf,
		size_t len, loff_t *pos)
{
	size_t count = 0;
	struct himax_ts_data *ts = private_ts;
	char *temp_buf = NULL;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kcalloc(len, sizeof(char), GFP_KERNEL);
		if (temp_buf != NULL) {
			count = snprintf(temp_buf, PAGE_SIZE, "%d\n",
					ts->SMWP_enable);

			if (copy_to_user(buf, temp_buf, len))
				I("%s, here:%d\n", __func__, __LINE__);

			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
		} else {
			E("%s, Failed to allocate memory\n", __func__);
		}
	} else {
		HX_PROC_SEND_FLAG = 0;
	}

	return count;
}

static ssize_t himax_SMWP_write(struct file *file, const char *buff,
		size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf[80] = {0};

	if (len >= 80) {
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

static ssize_t himax_GESTURE_read(struct file *file, char *buf,
		size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	int i = 0;
	size_t ret = 0;
	char *temp_buf = NULL;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kcalloc(len, sizeof(char), GFP_KERNEL);
		if (temp_buf != NULL) {
			for (i = 0; i < GEST_SUP_NUM; i++)
				ret += snprintf(temp_buf + ret, len - ret,
						"ges_en[%d]=%d\n",
						i, ts->gesture_cust_en[i]);

			if (copy_to_user(buf, temp_buf, len))
				I("%s, here:%d\n", __func__, __LINE__);

			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
		} else {
			E("%s, Failed to allocate memory\n", __func__);
		}
	} else {
		HX_PROC_SEND_FLAG = 0;
		ret = 0;
	}

	return ret;
}

static ssize_t himax_GESTURE_write(struct file *file, const char *buff,
		size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	int i = 0;
	int j = 0;
	char buf[80] = {0};

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	I("himax_GESTURE_store= %s, len = %d\n", buf, (int)len);

	for (i = 0; i < len; i++) {
		if (buf[i] == '0' && j < GEST_SUP_NUM) {
			ts->gesture_cust_en[j] = 0;
			I("gesture en[%d]=%d\n", j, ts->gesture_cust_en[j]);
			j++;
		} else if (buf[i] == '1' && j < GEST_SUP_NUM) {
			ts->gesture_cust_en[j] = 1;
			I("gesture en[%d]=%d\n", j, ts->gesture_cust_en[j]);
			j++;
		} else
			I("Not 0/1 or >=GEST_SUP_NUM : buf[%d] = %c\n",
				i, buf[i]);
	}

	return len;
}

static const struct file_operations himax_proc_Gesture_ops = {
	.owner = THIS_MODULE,
	.read = himax_GESTURE_read,
	.write = himax_GESTURE_write,
};

#if defined(HX_ULTRA_LOW_POWER)
static ssize_t himax_psensor_read(struct file *file, char *buf,
		size_t len, loff_t *pos)
{
	size_t count = 0;
	struct himax_ts_data *ts = private_ts;
	char *temp_buf = NULL;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kcalloc(len, sizeof(char), GFP_KERNEL);
		if (temp_buf != NULL) {
			count = snprintf(temp_buf, PAGE_SIZE,
					"p-sensor flag = %d\n",
					ts->psensor_flag);

			if (copy_to_user(buf, temp_buf, len))
				I("%s, here:%d\n", __func__, __LINE__);

			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
		} else {
			E("%s, Failed to allocate memory\n", __func__);
		}
	} else {
		HX_PROC_SEND_FLAG = 0;
	}

	return count;
}

static ssize_t himax_psensor_write(struct file *file, const char *buff,
		size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf[80] = {0};

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == '0' && ts->SMWP_enable == 1) {
		ts->psensor_flag = false;
		g_core_fp.fp_black_gest_ctrl(false);
	} else if (buf[0] == '1' && ts->SMWP_enable == 1) {
		ts->psensor_flag = true;
		g_core_fp.fp_black_gest_ctrl(true);
	} else if (ts->SMWP_enable == 0) {
		I("%s: SMWP is disable, not supprot to ctrl p-sensor.\n",
			__func__);
	} else
		return -EINVAL;

	I("%s: psensor_flag = %d.\n", __func__, ts->psensor_flag);
	return len;
}

static const struct file_operations himax_proc_psensor_ops = {
	.owner = THIS_MODULE,
	.read = himax_psensor_read,
	.write = himax_psensor_write,
};
#endif
#endif

#if defined(XM_GAME_MODE)

static ssize_t himax_gamemode_read(struct file *file, char *buf,
		size_t len, loff_t *pos)
{

	size_t ret = 0;
	char *temp_buf = NULL;
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_orientation_data[DATA_LEN_4];

	himax_parse_assign_cmd(game_mode_touchfunc_addr,tmp_addr,DATA_LEN_4);
	g_core_fp.fp_register_read(tmp_addr, tmp_data,DATA_LEN_4);

	himax_parse_assign_cmd(screen_orientation_addr,tmp_addr,DATA_LEN_4);
	g_core_fp.fp_register_read(tmp_addr, tmp_orientation_data,DATA_LEN_4);

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kcalloc(len, sizeof(char), GFP_KERNEL);

		if (temp_buf != NULL) {
			if (tmp_data[3] ==0xA5)
				ret += snprintf(temp_buf + ret, len - ret,"Game mode turn on \n");
			else
				ret += snprintf(temp_buf + ret, len - ret,"Game mode turn off \n");

			ret += snprintf(temp_buf + ret, len - ret,
					"himax_touch_up_threshold level set = %d\n", tmp_data[0]>>4);

			ret += snprintf(temp_buf + ret, len - ret,
					"himax_touch_Tolerance_threshold =  %d\n", tmp_data[0]&0x0F);

			ret += snprintf(temp_buf + ret, len - ret,
					"himax_touch_tap_stability =  %d\n", tmp_data[1]>>4);

			ret += snprintf(temp_buf + ret, len - ret,
					"himax_touch_aim_sensitivity =  %d\n", tmp_data[1]&0x0F);

			ret += snprintf(temp_buf + ret, len - ret,
					"himax_touch_border_prevent =  %d\n", tmp_data[2]);

			if (tmp_orientation_data[3] == 0xA5	&& tmp_orientation_data[2] == 0x5A
				&& tmp_orientation_data[1] == 0xA5 && tmp_orientation_data[0] == 0x5A)
				ret += snprintf(temp_buf + ret, len - ret,"normal portrait orientation \n");

			else if(tmp_orientation_data[3] == 0xA7	&& tmp_orientation_data[2] == 0x7A
				&& tmp_orientation_data[1] == 0xA7 && tmp_orientation_data[0] == 0x7A)
				ret += snprintf(temp_buf + ret, len - ret,"anticlockwise 180 degrees \n");

			else if(tmp_orientation_data[3] == 0xA1	&& tmp_orientation_data[2] == 0x1A
				&& tmp_orientation_data[1] == 0xA1 && tmp_orientation_data[0] == 0x1A)
				ret += snprintf(temp_buf + ret, len - ret,"anticlockwise 90 degrees \n");

			else if(tmp_orientation_data[3] == 0xA3	&& tmp_orientation_data[2] == 0x3A
				&& tmp_orientation_data[1] == 0xA3 && tmp_orientation_data[0] == 0x3A)
				ret += snprintf(temp_buf + ret, len - ret,"anticlockwise 270 degrees \n");

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);

			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
		} else {
			E("%s, Failed to allocate memory\n", __func__);
		}
	} else {
		HX_PROC_SEND_FLAG = 0;
		ret = 0;
	}

	return ret;
}

static ssize_t himax_gamemode_write(struct file *file, const char *buff,
		size_t len, loff_t *pos)
{

	char buf[80] = {0};
	uint8_t himax_game_value[2];

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;
	switch(buf[0]) {
		case '0':
			himax_game_value[0] = Touch_Game_Mode;
			I("game mode type is Touch_Game_Mode\n");
			break;
		case '1':
			himax_game_value[0] = Touch_UP_THRESHOLD;
			I("game mode type is Touch_Up_THRESHOLD\n");
			break;
		case '2':
			himax_game_value[0] = Touch_Tolerance;
			I("game mode type is Touch_Tolerance\n");
			break;
		case '3':
			himax_game_value[0] = Touch_Tap_Stability;
			I("game mode type is Touch_Tap_Stability\n");
			break;
		case '4':
			himax_game_value[0] = Touch_Aim_Sensitivity;
			I("game mode type is Touch_Aim_Sensitivity\n");
			break;
		case '5':
			himax_game_value[0] = Touch_Edge_Filter;
			I("game mode type is Touch_Edge_Filter\n");
			break;
		case '6':
			himax_game_value[0] = Touch_Panel_Orientation;
			I("screen orientation set horizontal notch left side");
			break;
		default:
			E("%s, not support this game mode type\n", __func__);
			break;
		}
	switch(buf[1]) {
		case '0':
			himax_game_value[1] = 0;
			I("game mode type level is 0\n");
			break;
		case '1':
			himax_game_value[1] = 1;
			I("game mode type level is 1\n");
			break;
		case '2':
			himax_game_value[1] = 2;
			I("game mode type level is 2\n");
			break;
		case '3':
			himax_game_value[1] = 3;
			I("game mode type level is 3\n");
			break;
		case '4':
			himax_game_value[1] = 4;
			I("game mode type level is 4\n");
			break;
		default:
			E("%s, not support this level\n", __func__);
			break;
		}

	g_core_fp.fp_game_mode_set(himax_game_value);

	return len;
}

static const struct file_operations himax_proc_game_mode_ops = {
	.owner = THIS_MODULE,
	.read = himax_gamemode_read,
	.write = himax_gamemode_write,
};
#endif

#if defined(HX_PEN_SWITCH)
static ssize_t himax_PSWITCH_read(struct file *file, char *buf,
		size_t len, loff_t *pos)
{
	uint8_t tmp_addr[3][DATA_LEN_4];
	uint8_t tmp_data[3][DATA_LEN_4] = {0x00};
	uint32_t reg_addr[3]={pen_mode_touchfunc_addr,pen_mode_restriction_addr,pen_charge_detect_addr};
	int i=0;
	size_t count = 0;
	char *temp_buf = NULL;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kcalloc(len, sizeof(char), GFP_KERNEL);
		if (temp_buf != NULL) {
			for(i=0;i<3;i++)
			{
				himax_parse_assign_cmd(reg_addr[i],tmp_addr[i],DATA_LEN_4);
				g_core_fp.fp_register_read(tmp_addr[i], tmp_data[i],DATA_LEN_4);
				switch(i)
				{
					case 0:
						if(tmp_data[i][0]==0x00 && memcmp(&tmp_data[i][0],&tmp_data[i][1],DATA_LEN_4-1)==0)
							count += snprintf(temp_buf, PAGE_SIZE, "pen switch is ON\n");
						else
							count += snprintf(temp_buf, PAGE_SIZE, "pen switch is OFF\n");
						break;
					case 1:
						if(tmp_data[i][0]==0x00 && memcmp(&tmp_data[i][0],&tmp_data[i][1],DATA_LEN_4-1)==0)
							count += snprintf(temp_buf+count, PAGE_SIZE, "pen mode restriction is OFF\n");
						else
							count += snprintf(temp_buf+count, PAGE_SIZE, "pen mode restriction is ON\n");
						break;
					case 2:
						if(tmp_data[i][0]==0x00 && memcmp(&tmp_data[i][0],&tmp_data[i][1],DATA_LEN_4-1)==0)
							count += snprintf(temp_buf+count, PAGE_SIZE, "pen charge mode state is OFF\n");
						else
							count += snprintf(temp_buf+count, PAGE_SIZE, "pen charge mode state is ON\n");
						break;
					default:
						break;
				}
					I("tmp_addr is R%02X%02X%02X%02XH = 0x%02X%02X%02X%02X\n",
					tmp_addr[i][3], tmp_addr[i][2], tmp_addr[i][1], tmp_addr[i][0],
					tmp_data[i][3], tmp_data[i][2], tmp_data[i][1], tmp_data[i][0]);
			}

			if (copy_to_user(buf, temp_buf, len))
				I("%s,here: %d\n", __func__, __LINE__);

			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
		} else {
			E("%s, Failed to allocate memory\n", __func__);
		}
	} else {
		HX_PROC_SEND_FLAG = 0;
	}

	return count;
}

static ssize_t himax_PSWITCH_write(struct file *file, const char *buff,
		size_t len, loff_t *pos)
{
	char buf[80] = {0};
	uint8_t himax_game_value[2];

	himax_game_value[0]=Touch_Pen_ENABLE;
	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == '0'){
		himax_game_value[1]=0;
		xiaomi_touch_interfaces.touch_mode[Touch_Pen_ENABLE][SET_CUR_VALUE] = 0;
	}
	else if (buf[0] == '1'){
		himax_game_value[1]=1;
		xiaomi_touch_interfaces.touch_mode[Touch_Pen_ENABLE][SET_CUR_VALUE] = 1;
	}
	else
		return -EINVAL;

	g_core_fp.fp_game_mode_set(himax_game_value);

	return len;
}

static const struct file_operations himax_proc_PSWITCH_ops = {
	.owner = THIS_MODULE,
	.read = himax_PSWITCH_read,
	.write = himax_PSWITCH_write,
};
#endif

#if defined(HX_PEN_CONNECT_STRATEGY)
static ssize_t pen_connect_strategy_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{

	size_t count = 0;

		if (buf != NULL) {
			count += snprintf(buf, PAGE_SIZE, "%d ",xiaomi_touch_interfaces.touch_mode[Touch_Pen_ENABLE][SET_CUR_VALUE]);
			count += snprintf(buf + count, PAGE_SIZE, "%d\n",private_ts->pen_is_charge);

			I("%s,here: %d\n", __func__, __LINE__);

		} else {
			E("%s, buf is NULL\n", __func__);
		}
	return count;
}

static DEVICE_ATTR_RO(pen_connect_strategy);

static int sysfs_node_init()
{
	int ret =0;
	sys_class=class_create(THIS_MODULE,HIMAX_SYS_NODE);

	if(!sys_class){
		ret = -1;
		E("sys/class/himax create failed!\n");
		return ret;
	}

	private_ts->dev=device_create(sys_class,
                      NULL,
                      MKDEV(0,0),
                      (void *)NULL,
                      "%s%d",HIMAX_SYS_NODE, 0);

    device_create_file(private_ts->dev,&dev_attr_pen_connect_strategy);
    return ret;
}
#endif

static ssize_t himax_vendor_read(struct file *file, char *buf,
				size_t len, loff_t *pos)
{
	ssize_t ret = 0;
	char *temp_buf = NULL;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kcalloc(len, sizeof(char), GFP_KERNEL);
		ret += snprintf(temp_buf + ret, len - ret,
				"IC = %s\n", private_ts->chip_name);

		ret += snprintf(temp_buf + ret, len - ret,
				"FW_VER = 0x%2.2X\n", ic_data->vendor_fw_ver);

		if (private_ts->chip_cell_type == CHIP_IS_ON_CELL) {
			ret += snprintf(temp_buf + ret, len - ret,
					"CONFIG_VER = 0x%2.2X\n",
					ic_data->vendor_config_ver);
		} else {
			ret += snprintf(temp_buf + ret, len - ret,
					"TOUCH_VER = 0x%2.2X\n",
					ic_data->vendor_touch_cfg_ver);
			ret += snprintf(temp_buf + ret, len - ret,
					"DISPLAY_VER = 0x%2.2X\n",
					ic_data->vendor_display_cfg_ver);
		}

		if (ic_data->vendor_cid_maj_ver < 0
		&& ic_data->vendor_cid_min_ver < 0) {
			ret += snprintf(temp_buf + ret, len - ret,
					"CID_VER = NULL\n");
		} else {
			ret += snprintf(temp_buf + ret, len - ret,
					"CID_VER = 0x%2.2X\n",
					(ic_data->vendor_cid_maj_ver << 8 |
					ic_data->vendor_cid_min_ver));
		}

		if (ic_data->vendor_panel_ver < 0) {
			ret += snprintf(temp_buf + ret, len - ret,
					"PANEL_VER = NULL\n");
		} else {
			ret += snprintf(temp_buf + ret, len - ret,
					"PANEL_VER = 0x%2.2X\n",
					ic_data->vendor_panel_ver);
		}
		if (private_ts->chip_cell_type == CHIP_IS_IN_CELL) {
			ret += snprintf(temp_buf + ret, len - ret,
					"Cusomer = %s\n",
					ic_data->vendor_cus_info);
			ret += snprintf(temp_buf + ret, len - ret,
					"Project = %s\n",
					ic_data->vendor_proj_info);
		}
		ret += snprintf(temp_buf + ret, len - ret, "\n");
		ret += snprintf(temp_buf + ret, len - ret,
				"Himax Touch Driver Version:\n");
		ret += snprintf(temp_buf + ret, len - ret, "%s\n",
				HIMAX_DRIVER_VER);

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else {
		HX_PROC_SEND_FLAG = 0;
	}

	return ret;
}
static const struct file_operations himax_proc_vendor_ops = {
	.owner = THIS_MODULE,
	.read = himax_vendor_read,
};

int himax_common_proc_init(void)
{
	himax_touch_proc_dir = proc_mkdir(HIMAX_PROC_TOUCH_FOLDER, NULL);

	if (himax_touch_proc_dir == NULL) {
		E(" %s: himax_touch_proc_dir file create failed!\n", __func__);
		return -ENOMEM;
	}
#if defined(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)
	if (fp_himax_self_test_init != NULL)
		fp_himax_self_test_init();
#endif

	himax_proc_self_test_file = proc_create(HIMAX_PROC_SELF_TEST_FILE, 0444,
		himax_touch_proc_dir, &himax_proc_self_test_ops);
	if (himax_proc_self_test_file == NULL) {
		E(" %s: proc self_test file create failed!\n", __func__);
		goto fail_1;
	}

#if defined(HX_HIGH_SENSE)
	himax_proc_HSEN_file = proc_create(HIMAX_PROC_HSEN_FILE, 0666,
			himax_touch_proc_dir, &himax_proc_HSEN_ops);

	if (himax_proc_HSEN_file == NULL) {
		E(" %s: proc HSEN file create failed!\n", __func__);
		goto fail_2;
	}

#endif
#if defined(HX_SMART_WAKEUP)
	himax_proc_SMWP_file = proc_create(HIMAX_PROC_SMWP_FILE, 0666,
			himax_touch_proc_dir, &himax_proc_SMWP_ops);

	if (himax_proc_SMWP_file == NULL) {
		E(" %s: proc SMWP file create failed!\n", __func__);
		goto fail_3;
	}

	himax_proc_GESTURE_file = proc_create(HIMAX_PROC_GESTURE_FILE, 0666,
			himax_touch_proc_dir, &himax_proc_Gesture_ops);

	if (himax_proc_GESTURE_file == NULL) {
		E(" %s: proc GESTURE file create failed!\n", __func__);
		goto fail_4;
	}
#if defined(HX_ULTRA_LOW_POWER)
	himax_proc_psensor_file = proc_create(HIMAX_PROC_PSENSOR_FILE, 0666,
			himax_touch_proc_dir, &himax_proc_psensor_ops);

	if (himax_proc_psensor_file == NULL) {
		E(" %s: proc GESTURE file create failed!\n", __func__);
		goto fail_5;
	}
#endif
#endif
	himax_proc_vendor_file = proc_create(HIMAX_PROC_VENDOR_FILE, 0444,
		himax_touch_proc_dir, &himax_proc_vendor_ops);
	if (himax_proc_vendor_file == NULL) {
		E(" %s: proc vendor file create failed!\n", __func__);
		goto fail_6;
	}
#if defined(XM_GAME_MODE)
	himax_proc_gamemode_file = proc_create(HIMAX_PROC_GAMEMODE_FILE, 0444,
		himax_touch_proc_dir, &himax_proc_game_mode_ops);
	if (himax_proc_vendor_file == NULL) {
		E(" %s: proc vendor file create failed!\n", __func__);
		goto fail_7;
	}
#endif
#if defined(HX_PEN_SWITCH)
	himax_proc_PSWITCH_file = proc_create(HIMAX_PROC_PSWITCH_FILE, 0666,
			himax_touch_proc_dir, &himax_proc_PSWITCH_ops);
		if (himax_proc_PSWITCH_file == NULL) {
		E(" %s: proc PEN SWITCH file create failed!\n", __func__);
		goto fail_8;
	}

#endif

	xiaomi_proc_self_test_file = proc_create(XIAOMI_PROC_SELF_TEST_FILE, 0444,
		NULL, &himax_proc_self_test_ops);
	if (xiaomi_proc_self_test_file == NULL) {
		E(" %s: proc tp_selftest file create failed!\n", __func__);
		goto fail_9;
	}

	return 0;

	remove_proc_entry(XIAOMI_PROC_SELF_TEST_FILE, NULL);
fail_9:
#if defined(HX_PEN_SWITCH)
	remove_proc_entry(HIMAX_PROC_PSWITCH_FILE, himax_touch_proc_dir);
fail_8:
#endif
#if defined(XM_GAME_MODE)
	remove_proc_entry(HIMAX_PROC_GAMEMODE_FILE, himax_touch_proc_dir);
fail_7:
#endif
	remove_proc_entry(HIMAX_PROC_VENDOR_FILE, himax_touch_proc_dir);
fail_6:
#if defined(HX_SMART_WAKEUP)
#if defined(HX_ULTRA_LOW_POWER)
	remove_proc_entry(HIMAX_PROC_PSENSOR_FILE, himax_touch_proc_dir);
fail_5:
#endif
	remove_proc_entry(HIMAX_PROC_GESTURE_FILE, himax_touch_proc_dir);
fail_4:
	remove_proc_entry(HIMAX_PROC_SMWP_FILE, himax_touch_proc_dir);
fail_3:
#endif
#if defined(HX_HIGH_SENSE)
	remove_proc_entry(HIMAX_PROC_HSEN_FILE, himax_touch_proc_dir);
fail_2:
#endif
	remove_proc_entry(HIMAX_PROC_SELF_TEST_FILE, himax_touch_proc_dir);
fail_1:
	return -ENOMEM;
}

void himax_common_proc_deinit(void)
{
	remove_proc_entry(HIMAX_PROC_VENDOR_FILE, himax_touch_proc_dir);
#if defined(HX_SMART_WAKEUP)
#if defined(HX_ULTRA_LOW_POWER)
	remove_proc_entry(HIMAX_PROC_PSENSOR_FILE, himax_touch_proc_dir);
#endif
	remove_proc_entry(HIMAX_PROC_GESTURE_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_SMWP_FILE, himax_touch_proc_dir);
#endif
#if defined(HX_HIGH_SENSE)
	remove_proc_entry(HIMAX_PROC_HSEN_FILE, himax_touch_proc_dir);
#endif
	remove_proc_entry(HIMAX_PROC_SELF_TEST_FILE, himax_touch_proc_dir);

#if defined(XM_GAME_MODE)
	remove_proc_entry(HIMAX_PROC_GAMEMODE_FILE, himax_touch_proc_dir);
#endif
#if defined(HX_PEN_SWITCH)
	remove_proc_entry(HIMAX_PROC_PSWITCH_FILE, himax_touch_proc_dir);
#endif

	remove_proc_entry(HIMAX_PROC_TOUCH_FOLDER, NULL);
	remove_proc_entry(XIAOMI_PROC_SELF_TEST_FILE, NULL);
}


void himax_parse_assign_cmd(uint32_t addr, uint8_t *cmd, int len)
{

	switch (len) {
	case 1:
		cmd[0] = addr;
		break;

	case 2:
		cmd[0] = addr & 0xFF;
		cmd[1] = (addr >> 8) & 0xFF;
		break;

	case 4:
		cmd[0] = addr & 0xFF;
		cmd[1] = (addr >> 8) & 0xFF;
		cmd[2] = (addr >> 16) & 0xFF;
		cmd[3] = (addr >> 24) & 0xFF;
		break;

	default:
		E("%s: input length fault,len = %d!\n", __func__, len);
	}
}
EXPORT_SYMBOL(himax_parse_assign_cmd);

int himax_input_register(struct himax_ts_data *ts)
{
	int ret = 0;
#if defined(HX_SMART_WAKEUP)
	int i = 0;
#endif
	ret = himax_dev_set(ts);
	if (ret < 0) {
		I("%s, input device register fail!\n", __func__);
		return INPUT_REGISTER_FAIL;
	}

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(KEY_BACK, ts->input_dev->keybit);
	set_bit(KEY_HOME, ts->input_dev->keybit);
	set_bit(KEY_MENU, ts->input_dev->keybit);
	set_bit(KEY_SEARCH, ts->input_dev->keybit);

#if defined(HX_SMART_WAKEUP)
	for (i = 0; i < GEST_SUP_NUM; i++)
		set_bit(gest_key_def[i], ts->input_dev->keybit);
#elif defined(CONFIG_TOUCHSCREEN_HIMAX_INSPECT) || defined(HX_PALM_REPORT)
	set_bit(KEY_POWER, ts->input_dev->keybit);
#endif
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(KEY_APPSELECT, ts->input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
#if defined(HX_PROTOCOL_A)
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 1, 10, 0, 0);
#else
	set_bit(MT_TOOL_FINGER, ts->input_dev->keybit);
#if defined(HX_PROTOCOL_B_3PA)
	input_mt_init_slots(ts->input_dev, ts->nFinger_support,
			INPUT_MT_DIRECT);
#else
	input_mt_init_slots(ts->input_dev, ts->nFinger_support);
#endif
#endif
	I("input_set_abs_params: mix_x %d, max_x %d, min_y %d, max_y %d\n",
			ts->pdata->abs_x_min,
			ts->pdata->abs_x_max,
			ts->pdata->abs_y_min,
			ts->pdata->abs_y_max);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
			ts->pdata->abs_x_min, ((ts->pdata->abs_x_max+1)*ic_data->HX_TOUCHSCREEN_RATIO-1),
			ts->pdata->abs_x_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
			ts->pdata->abs_y_min, ((ts->pdata->abs_y_max+1)*ic_data->HX_TOUCHSCREEN_RATIO-1),
			ts->pdata->abs_y_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
			ts->pdata->abs_pressure_min,
			ts->pdata->abs_pressure_max,
			ts->pdata->abs_pressure_fuzz, 0);
#if !defined(HX_PROTOCOL_A)
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE,
			ts->pdata->abs_pressure_min,
			ts->pdata->abs_pressure_max,
			ts->pdata->abs_pressure_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR,
			ts->pdata->abs_width_min,
			ts->pdata->abs_width_max,
			ts->pdata->abs_pressure_fuzz, 0);
#endif

	if (himax_input_register_device(ts->input_dev) == 0) {
		ret = NO_ERR;
	} else {
		E("%s: input register fail\n", __func__);
		input_free_device(ts->input_dev);
		return INPUT_REGISTER_FAIL;
	}

	if (!ic_data->HX_STYLUS_FUNC)
		goto skip_stylus_operation;

	set_bit(EV_SYN, ts->stylus_dev->evbit);
	set_bit(EV_ABS, ts->stylus_dev->evbit);
	set_bit(EV_KEY, ts->stylus_dev->evbit);
	set_bit(BTN_TOUCH, ts->stylus_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, ts->stylus_dev->propbit);

	set_bit(BTN_TOOL_PEN, ts->stylus_dev->keybit);
	set_bit(BTN_TOOL_RUBBER, ts->stylus_dev->keybit);

	input_set_abs_params(ts->stylus_dev, ABS_PRESSURE, 0, 4095, 0, 0);
	input_set_abs_params(ts->stylus_dev, ABS_DISTANCE, 0, 1, 0, 0);
	input_set_abs_params(ts->stylus_dev, ABS_TILT_X, -60, 60, 0, 0);
	input_set_abs_params(ts->stylus_dev, ABS_TILT_Y, -60, 60, 0, 0);
	input_set_capability(ts->stylus_dev, EV_KEY, BTN_TOUCH);
	input_set_capability(ts->stylus_dev, EV_KEY, BTN_STYLUS);
	input_set_capability(ts->stylus_dev, EV_KEY, BTN_STYLUS2);

	input_set_abs_params(ts->stylus_dev, ABS_X, ts->pdata->abs_x_min,
			((ts->pdata->abs_x_max+1)*ic_data->HX_STYLUS_RATIO-1),
			ts->pdata->abs_x_fuzz, 0);
	input_set_abs_params(ts->stylus_dev, ABS_Y, ts->pdata->abs_y_min,
			((ts->pdata->abs_y_max+1)*ic_data->HX_STYLUS_RATIO-1),
			ts->pdata->abs_y_fuzz, 0);

	if (himax_input_register_device(ts->stylus_dev) == 0) {
		ret = NO_ERR;
	} else {
		E("%s: input register stylus fail\n", __func__);
		input_unregister_device(ts->input_dev);
		input_free_device(ts->stylus_dev);
		return INPUT_REGISTER_FAIL;
	}

skip_stylus_operation:

	I("%s, input device registered.\n", __func__);

	return ret;
}
EXPORT_SYMBOL(himax_input_register);

#if defined(HX_BOOT_UPGRADE)
static int himax_get_fw_ver_bin(void)
{
	I("%s: use default incell address.\n", __func__);
	if (hxfw != NULL) {
		I("Catch fw version in bin file!\n");
		g_i_FW_VER = (hxfw->data[FW_VER_MAJ_FLASH_ADDR] << 8)
				| hxfw->data[FW_VER_MIN_FLASH_ADDR];
#if defined(CONFIG_TOUCHSCREEN_HIMAX_INCELL)
		g_i_CFG_VER = (hxfw->data[CFG_VER_MAJ_FLASH_ADDR] << 8)
				| hxfw->data[CFG_VER_MIN_FLASH_ADDR];
#else
		g_i_CFG_VER = hxfw->data[CFG_VER_MAJ_FLASH_ADDR];
#endif
		g_i_CID_MAJ = hxfw->data[CID_VER_MAJ_FLASH_ADDR];
		g_i_CID_MIN = hxfw->data[CID_VER_MIN_FLASH_ADDR];
	} else {
		I("FW data is null!\n");
		return 1;
	}
	return NO_ERR;
}

static int himax_auto_update_check(void)
{
	int32_t ret;

	if (himax_get_fw_ver_bin() == 0) {
		if (((ic_data->vendor_fw_ver != g_i_FW_VER)
		|| (ic_data->vendor_config_ver != g_i_CFG_VER))) {
			I("%s: Need update\n", __func__);
			ret = 0;
		} else {
			I("%s: Need not update!\n", __func__);
			ret = 1;
		}
	} else {
		E("%s: FW bin fail!\n", __func__);
		ret = 1;
	}

	return ret;
}
#endif

#if defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH)
static int i_get_FW(void)
{
	int ret = -1;
	int result = NO_ERR;

	ret = request_firmware(&hxfw, g_fw_boot_upgrade_name, private_ts->dev);
	I("%s: request file %s finished\n", __func__, g_fw_boot_upgrade_name);
	if (ret < 0) {
#if defined(__EMBEDDED_FW__)
		hxfw = &g_embedded_fw;
		I("%s: Not find FW in userspace, use embedded FW(size:%zu)",
			__func__, g_embedded_fw.size);
		result = HX_EMBEDDED_FW;
#else
		E("%s,%d: error code = %d\n", __func__, __LINE__, ret);
		return OPEN_FILE_FAIL;
#endif
	}

	return result;
}

static int i_update_FW(void)
{
	int upgrade_times = 0;
	int8_t ret = 0;
	int8_t result = 0;

update_retry:
#if defined(HX_ZERO_FLASH)

	ret = g_core_fp.fp_firmware_update_0f(hxfw, 0);
	if (ret != 0) {
		upgrade_times++;
		E("%s: TP upgrade error, upgrade_times = %d\n",
				__func__, upgrade_times);

		if (upgrade_times < 3)
			goto update_retry;
		else
			result = -1;

	} else {
		result = 1;
		I("%s: TP upgrade OK\n", __func__);
	}

#else

	if (hxfw->size == FW_SIZE_32k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_32k(
			(unsigned char *)hxfw->data, hxfw->size, false);
	else if (hxfw->size == FW_SIZE_60k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_60k(
			(unsigned char *)hxfw->data, hxfw->size, false);
	else if (hxfw->size == FW_SIZE_64k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k(
			(unsigned char *)hxfw->data, hxfw->size, false);
	else if (hxfw->size == FW_SIZE_124k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_124k(
			(unsigned char *)hxfw->data, hxfw->size, false);
	else if (hxfw->size == FW_SIZE_128k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k(
			(unsigned char *)hxfw->data, hxfw->size, false);
	else if (hxfw->size == FW_SIZE_255k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_255k(
			(unsigned char *)hxfw->data, hxfw->size, false);

	if (ret == 0) {
		upgrade_times++;
		E("%s: TP upgrade error, upgrade_times = %d\n",
				__func__, upgrade_times);

		if (upgrade_times < 3)
			goto update_retry;
		else
			result = -1;

	} else {
		result = 1;
		I("%s: TP upgrade OK\n", __func__);
	}
#endif

	return result;
}
#endif
#if defined(HX_EXCP_RECOVERY)
static void himax_excp_hw_reset(void)
{
#if defined(HX_ZERO_FLASH)
	int result = 0;
#endif
	I("%s: START EXCEPTION Reset\n", __func__);
#if defined(HX_ZERO_FLASH)
	result = g_core_fp.fp_0f_op_file_dirly(g_fw_boot_upgrade_name);
	if (result)
		E("%s: update FW fail, code[%d]!!\n", __func__, result);
#else
	g_core_fp.fp_excp_ic_reset();
#endif
	himax_report_all_leave_event(private_ts);
	I("%s: END EXCEPTION Reset\n", __func__);
}
#endif

#if defined(HX_SMART_WAKEUP)
#if defined(HX_GESTURE_TRACK)
static void gest_pt_log_coordinate(int rx, int tx)
{
	/*driver report x y with range 0 - 255 , we scale it up to x/y pixel*/
	gest_pt_x[gest_pt_cnt] = rx * (ic_data->HX_X_RES) / 255;
	gest_pt_y[gest_pt_cnt] = tx * (ic_data->HX_Y_RES) / 255;
}
#endif
static int himax_wake_event_parse(struct himax_ts_data *ts, int ts_status)
{
	uint8_t *buf = wake_event_buffer;
#if defined(HX_GESTURE_TRACK)
	int tmp_max_x = 0x00;
	int tmp_min_x = 0xFFFF;
	int tmp_max_y = 0x00;
	int tmp_min_y = 0xFFFF;
	int gest_len;
#endif
	int i = 0, check_FC = 0, ret;
	int j = 0, gesture_pos = 0, gesture_flag = 0;

	if (g_ts_dbg != 0)
		I("%s: Entering!, ts_status=%d\n", __func__, ts_status);

	if (buf == NULL) {
		ret = -ENOMEM;
		goto END;
	}

	memcpy(buf, hx_touch_data->hx_event_buf, hx_touch_data->event_size);

	for (i = 0; i < GEST_PTLG_ID_LEN; i++) {
		for (j = 0; j < GEST_SUP_NUM; j++) {
			if (buf[i] == gest_event[j]) {
				gesture_flag = buf[i];
				gesture_pos = j;
				break;
			}
		}
		I("0x%2.2X ", buf[i]);
		if (buf[i] == gesture_flag) {
			check_FC++;
		} else {
			I("ID START at %x , value = 0x%2X skip the event\n",
					i, buf[i]);
			break;
		}
	}

	I("Himax gesture_flag= %x\n", gesture_flag);
	I("Himax check_FC is %d\n", check_FC);

	if (check_FC != GEST_PTLG_ID_LEN) {
		ret = 0;
		goto END;
	}

	if (buf[GEST_PTLG_ID_LEN] != GEST_PTLG_HDR_ID1
	|| buf[GEST_PTLG_ID_LEN + 1] != GEST_PTLG_HDR_ID2) {
		ret = 0;
		goto END;
	}

#if defined(HX_GESTURE_TRACK)

	if (buf[GEST_PTLG_ID_LEN] == GEST_PTLG_HDR_ID1
	&& buf[GEST_PTLG_ID_LEN + 1] == GEST_PTLG_HDR_ID2) {
		gest_len = buf[GEST_PTLG_ID_LEN + 2];
		I("gest_len = %d\n", gest_len);
		i = 0;
		gest_pt_cnt = 0;
		I("gest doornidate start\n %s", __func__);

		while (i < (gest_len + 1) / 2) {
			gest_pt_log_coordinate(
					buf[GEST_PTLG_ID_LEN + 4 + i * 2],
					buf[GEST_PTLG_ID_LEN + 4 + i * 2 + 1]);
			i++;
			I("gest_pt_x[%d]=%d,gest_pt_y[%d]=%d\n",
				gest_pt_cnt,
				gest_pt_x[gest_pt_cnt],
				gest_pt_cnt,
				gest_pt_y[gest_pt_cnt]);
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

			I("gest_point x_min=%d,x_max=%d,y_min=%d,y_max=%d\n",
				tmp_min_x, tmp_max_x, tmp_min_y, tmp_max_y);

			gest_start_x = gest_pt_x[0];
			hx_gesture_coor[0] = gest_start_x;
			gest_start_y = gest_pt_y[0];
			hx_gesture_coor[1] = gest_start_y;
			gest_end_x = gest_pt_x[gest_pt_cnt - 1];
			hx_gesture_coor[2] = gest_end_x;
			gest_end_y = gest_pt_y[gest_pt_cnt - 1];
			hx_gesture_coor[3] = gest_end_y;
			gest_width = tmp_max_x - tmp_min_x;
			hx_gesture_coor[4] = gest_width;
			gest_height = tmp_max_y - tmp_min_y;
			hx_gesture_coor[5] = gest_height;
			gest_mid_x = (tmp_max_x + tmp_min_x) / 2;
			hx_gesture_coor[6] = gest_mid_x;
			gest_mid_y = (tmp_max_y + tmp_min_y) / 2;
			hx_gesture_coor[7] = gest_mid_y;
			hx_gesture_coor[8] = gest_mid_x;
			hx_gesture_coor[9] = gest_mid_y - gest_height / 2;
			hx_gesture_coor[10] = gest_mid_x;			
			hx_gesture_coor[11] = gest_mid_y + gest_height / 2;
			hx_gesture_coor[12] = gest_mid_x - gest_width / 2;
			hx_gesture_coor[13] = gest_mid_y;		
			hx_gesture_coor[14] = gest_mid_x + gest_width / 2;
			hx_gesture_coor[15] = gest_mid_y;
		}
	}

#endif

	if (!ts->gesture_cust_en[gesture_pos]) {
		I("%s NOT report key [%d] = %d\n", __func__,
				gesture_pos, gest_key_def[gesture_pos]);
		g_target_report_data->SMWP_event_chk = 0;
		ret = 0;
	} else {
		g_target_report_data->SMWP_event_chk =
				gest_key_def[gesture_pos];
		ret = gesture_pos;
	}
END:
	return ret;
}

static void himax_wake_event_report(void)
{
	int KEY_EVENT = g_target_report_data->SMWP_event_chk;

	if (g_ts_dbg != 0)
		I("%s: Entering!\n", __func__);

	if (KEY_EVENT) {
		I("%s SMART WAKEUP KEY event %d press\n", __func__, KEY_EVENT);
		input_report_key(private_ts->input_dev, KEY_EVENT, 1);
		input_sync(private_ts->input_dev);
		I("%s SMART WAKEUP KEY event %d release\n",
				__func__, KEY_EVENT);
		input_report_key(private_ts->input_dev, KEY_EVENT, 0);
		input_sync(private_ts->input_dev);
#if defined(HX_GESTURE_TRACK)
		I("gest_start_x=%d,start_y=%d,end_x=%d,end_y=%d\n",
			gest_start_x,
			gest_start_y,
			gest_end_x,
			gest_end_y);
		I("gest_width=%d,height=%d,mid_x=%d,mid_y=%d\n",
			gest_width,
			gest_height,
			gest_mid_x,
			gest_mid_y);
		I("gest_up_x=%d,up_y=%d,down_x=%d,down_y=%d\n",
			hx_gesture_coor[8],
			hx_gesture_coor[9],
			hx_gesture_coor[10],
			hx_gesture_coor[11]);
		I("gest_left_x=%d,left_y=%d,right_x=%d,right_y=%d\n",
			hx_gesture_coor[12],
			hx_gesture_coor[13],
			hx_gesture_coor[14],
			hx_gesture_coor[15]);
#endif
		g_target_report_data->SMWP_event_chk = 0;
	}
}

#endif

int himax_report_data_init(void)
{
	if (hx_touch_data->hx_coord_buf != NULL) {
		kfree(hx_touch_data->hx_coord_buf);
		hx_touch_data->hx_coord_buf = NULL;
	}

	if (hx_touch_data->hx_rawdata_buf != NULL) {
		kfree(hx_touch_data->hx_rawdata_buf);
		hx_touch_data->hx_rawdata_buf = NULL;
	}

#if defined(HX_SMART_WAKEUP)
	hx_touch_data->event_size = g_core_fp.fp_get_touch_data_size();

	if (hx_touch_data->hx_event_buf != NULL) {
		kfree(hx_touch_data->hx_event_buf);
		hx_touch_data->hx_event_buf = NULL;
	}

	if (wake_event_buffer != NULL) {
		kfree(wake_event_buffer);
		wake_event_buffer = NULL;
	}

#endif

	if (g_target_report_data != NULL) {
		if (ic_data->HX_STYLUS_FUNC) {
			kfree(g_target_report_data->s);
			g_target_report_data->s = NULL;
		}

		kfree(g_target_report_data->p);
		g_target_report_data->p = NULL;
		kfree(g_target_report_data);
		g_target_report_data = NULL;
	}

	hx_touch_data->touch_all_size = g_core_fp.fp_get_touch_data_size();
	hx_touch_data->raw_cnt_max = ic_data->HX_MAX_PT / 4;
	hx_touch_data->raw_cnt_rmd = ic_data->HX_MAX_PT % 4;
	if (hx_touch_data->raw_cnt_rmd != 0x00) {
		hx_touch_data->rawdata_size =
			g_core_fp.fp_cal_data_len(
				hx_touch_data->raw_cnt_rmd,
				ic_data->HX_MAX_PT,
				hx_touch_data->raw_cnt_max);

		hx_touch_data->touch_info_size = (ic_data->HX_MAX_PT
				+ hx_touch_data->raw_cnt_max + 2) * 4;
	} else {
		hx_touch_data->rawdata_size =
			g_core_fp.fp_cal_data_len(
				hx_touch_data->raw_cnt_rmd,
				ic_data->HX_MAX_PT,
				hx_touch_data->raw_cnt_max);

		hx_touch_data->touch_info_size = (ic_data->HX_MAX_PT
				+ hx_touch_data->raw_cnt_max + 1) * 4;
	}

	if (ic_data->HX_STYLUS_FUNC) {
		hx_touch_data->touch_info_size += STYLUS_INFO_SZ;
		hx_touch_data->rawdata_size -= STYLUS_INFO_SZ;
	}

	if ((ic_data->HX_TX_NUM
	* ic_data->HX_RX_NUM
	+ ic_data->HX_TX_NUM
	+ ic_data->HX_RX_NUM)
	% hx_touch_data->rawdata_size == 0)
		hx_touch_data->rawdata_frame_size =
			(ic_data->HX_TX_NUM
			* ic_data->HX_RX_NUM
			+ ic_data->HX_TX_NUM
			+ ic_data->HX_RX_NUM)
			/ hx_touch_data->rawdata_size;
	else
		hx_touch_data->rawdata_frame_size =
			(ic_data->HX_TX_NUM
			* ic_data->HX_RX_NUM
			+ ic_data->HX_TX_NUM
			+ ic_data->HX_RX_NUM)
			/ hx_touch_data->rawdata_size
			+ 1;


	hx_touch_data->hx_coord_buf = kzalloc(sizeof(uint8_t)
			* (hx_touch_data->touch_info_size),
			GFP_KERNEL);

	if (hx_touch_data->hx_coord_buf == NULL)
		goto mem_alloc_fail_coord_buf;

#if defined(HX_SMART_WAKEUP)
	wake_event_buffer = kcalloc(hx_touch_data->event_size,
			sizeof(uint8_t), GFP_KERNEL);
	if (wake_event_buffer == NULL)
		goto mem_alloc_fail_smwp;

	hx_touch_data->hx_event_buf = kzalloc(sizeof(uint8_t)
			* (hx_touch_data->event_size), GFP_KERNEL);
	if (hx_touch_data->hx_event_buf == NULL)
		goto mem_alloc_fail_event_buf;
#endif

	hx_touch_data->hx_rawdata_buf = kzalloc(sizeof(uint8_t)
		* (hx_touch_data->touch_all_size
		- hx_touch_data->touch_info_size),
		GFP_KERNEL);
	if (hx_touch_data->hx_rawdata_buf == NULL)
		goto mem_alloc_fail_rawdata_buf;

	if (g_target_report_data == NULL) {
		g_target_report_data =
			kzalloc(sizeof(struct himax_target_report_data),
			GFP_KERNEL);
		if (g_target_report_data == NULL)
			goto mem_alloc_fail_report_data;

		g_target_report_data->p = kzalloc(
			sizeof(struct himax_target_point_data)
			*(ic_data->HX_MAX_PT), GFP_KERNEL);
		if (g_target_report_data->p == NULL)
			goto mem_alloc_fail_report_data_p;

		if (!ic_data->HX_STYLUS_FUNC)
			goto skip_stylus_operation;

		g_target_report_data->s = kzalloc(
			sizeof(struct himax_target_stylus_data)*2, GFP_KERNEL);
		if (g_target_report_data->s == NULL)
			goto mem_alloc_fail_report_data_s;
	}

skip_stylus_operation:

	return NO_ERR;

mem_alloc_fail_report_data_s:
	kfree(g_target_report_data->p);
	g_target_report_data->p = NULL;
mem_alloc_fail_report_data_p:
	kfree(g_target_report_data);
	g_target_report_data = NULL;
mem_alloc_fail_report_data:
	kfree(hx_touch_data->hx_rawdata_buf);
	hx_touch_data->hx_rawdata_buf = NULL;
mem_alloc_fail_rawdata_buf:
#if defined(HX_SMART_WAKEUP)
	kfree(hx_touch_data->hx_event_buf);
	hx_touch_data->hx_event_buf = NULL;
mem_alloc_fail_event_buf:
	kfree(wake_event_buffer);
	wake_event_buffer = NULL;
mem_alloc_fail_smwp:
#endif
	kfree(hx_touch_data->hx_coord_buf);
	hx_touch_data->hx_coord_buf = NULL;
mem_alloc_fail_coord_buf:

	E("%s: Failed to allocate memory\n", __func__);
	return MEM_ALLOC_FAIL;
}
EXPORT_SYMBOL(himax_report_data_init);

void himax_report_data_deinit(void)
{
	if (ic_data->HX_STYLUS_FUNC) {
		kfree(g_target_report_data->s);
		g_target_report_data->s = NULL;
	}

	kfree(g_target_report_data->p);
	g_target_report_data->p = NULL;
	kfree(g_target_report_data);
	g_target_report_data = NULL;

#if defined(HX_SMART_WAKEUP)
	kfree(wake_event_buffer);
	wake_event_buffer = NULL;
	kfree(hx_touch_data->hx_event_buf);
	hx_touch_data->hx_event_buf = NULL;
#endif
	kfree(hx_touch_data->hx_rawdata_buf);
	hx_touch_data->hx_rawdata_buf = NULL;
	kfree(hx_touch_data->hx_coord_buf);
	hx_touch_data->hx_coord_buf = NULL;
}

#if defined(HX_USB_DETECT_GLOBAL)
void himax_cable_detect_func(bool force_renew)
{
	struct himax_ts_data *ts;

	uint8_t connect_status = 0;

	connect_status = USB_detect_flag;
	ts = private_ts;

	if (ts->cable_config) {
		if ((connect_status != ts->usb_connected) || force_renew) {
			if (connect_status) {
				ts->cable_config[1] = 0x01;
				ts->usb_connected = 0x01;
			} else {
				ts->cable_config[1] = 0x00;
				ts->usb_connected = 0x00;
			}

			g_core_fp.fp_usb_detect_set(ts->cable_config);
			I("%s: Cable status change: 0x%2.2X\n", __func__,
					ts->usb_connected);
		}
	}
}
static void himax_power_supply_work(struct work_struct *work)
{
	int connect_status = 0;

	mutex_lock(&private_ts->power_supply_lock);
	if (bTouchIsAwake) {
		connect_status = !!power_supply_is_system_supplied();
		if (private_ts->cable_config){
			if ((connect_status != private_ts->usb_exist) || private_ts->usb_exist < 0) {
			private_ts->usb_exist = connect_status;
			if (connect_status) {
				private_ts->cable_config[1] = 0x01;
			} else {
				private_ts->cable_config[1] = 0x00;
			}
			g_core_fp.fp_usb_detect_set(private_ts->cable_config);
			I("%s: Cable status change: 0x%2.2X\n", __func__,
					private_ts->usb_exist);
		}
		}
	}
	mutex_unlock(&private_ts->power_supply_lock);

}

static int himax_power_supply_event(struct notifier_block *nb,
				  unsigned long event, void *ptr)
{
	if (private_ts && &private_ts ->himax_supply_work != NULL && private_ts ->event_wq != NULL)
		queue_work(private_ts ->event_wq, &private_ts ->himax_supply_work);
	return 0;
}
#endif

#if defined(HX_PEN_DETECT_GLOBAL)
static void himax_pen_supply_work(struct work_struct *work)
{
	int ret=0;
	int pen_connect_flag = xiaomi_touch_interfaces.touch_mode[Touch_Pen_ENABLE][GET_CUR_VALUE];

		mutex_lock(&private_ts->pen_supply_lock);
		if(bTouchIsAwake){
			if(private_ts->pen_exist != private_ts->pen_is_charge || private_ts->pen_exist == -1){
				private_ts->pen_exist=private_ts->pen_is_charge;
				if(pen_connect_flag == 1){
					ret=g_core_fp.fp_pen_charge_mode_set(private_ts->pen_is_charge);
				}else{
					I("Bluetooth is not connected to the stylus, skip the magnetic attraction logic\n");
				}
				if(ret<0)
					E("pen charge mode set fail!!%d\n");
			}
		}
		mutex_unlock(&private_ts->pen_supply_lock);
		return;
}

static int himax_pen_supply_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	private_ts->pen_is_charge = event;
	schedule_work(&private_ts->pen_supply_change_work);
	return 0;
}
#endif

static int himax_ts_work_status(struct himax_ts_data *ts)
{
	int result = HX_REPORT_COORD;
	hx_touch_data->diag_cmd = ts->diag_cmd;
	if (hx_touch_data->diag_cmd)
		result = HX_REPORT_COORD_RAWDATA;

#if defined(HX_SMART_WAKEUP)
	if (atomic_read(&ts->suspend_mode)
	&& (ts->SMWP_enable)
	&& (!hx_touch_data->diag_cmd))
		result = HX_REPORT_SMWP_EVENT;
#endif
	return result;
}

static int himax_touch_get(struct himax_ts_data *ts, uint8_t *buf,
		int ts_path, int ts_status)
{
	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	switch (ts_path) {
	case HX_REPORT_COORD:
		if ((HX_HW_RESET_ACTIVATE)
#if defined(HX_EXCP_RECOVERY)
			|| (HX_EXCP_RESET_ACTIVATE)
#endif
			) {
			if (!g_core_fp.fp_read_event_stack(buf, 128)) {
				E("%s: can't read data from chip!\n", __func__);
				ts_status = HX_TS_GET_DATA_FAIL;
			}
		} else {
			if (!g_core_fp.fp_read_event_stack(buf,
			hx_touch_data->touch_info_size)) {
				E("%s: can't read data from chip!\n", __func__);
				ts_status = HX_TS_GET_DATA_FAIL;
			}
		}
		break;
#if defined(HX_SMART_WAKEUP)
	case HX_REPORT_SMWP_EVENT:
		__pm_wakeup_event(ts->ts_SMWP_wake_lock, TS_WAKE_LOCK_TIMEOUT);
		msleep(20);

		if (!g_core_fp.fp_read_event_stack(buf,
		hx_touch_data->event_size)) {
			E("%s: can't read data from chip!\n", __func__);
			ts_status = HX_TS_GET_DATA_FAIL;
		}
		break;
#endif
	case HX_REPORT_COORD_RAWDATA:
		if (!g_core_fp.fp_read_event_stack(buf, 128)) {
			E("%s: can't read data from chip!\n", __func__);
			ts_status = HX_TS_GET_DATA_FAIL;
		}
		break;
	default:
		break;
	}

	return ts_status;
}

static int himax_checksum_cal(struct himax_ts_data *ts, uint8_t *buf,
		int ts_path, int ts_status)
{
	uint16_t check_sum_cal = 0;
	int32_t	i = 0;
	int length = 0;
	int zero_cnt = 0;
	int raw_data_sel = 0;
	int ret_val = ts_status;

	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	switch (ts_path) {
	case HX_REPORT_COORD:
		length = hx_touch_data->touch_info_size;
		break;
#if defined(HX_SMART_WAKEUP)
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

	for (i = 0; i < length; i++) {
		check_sum_cal += buf[i];
		if (buf[i] == 0x00)
			zero_cnt++;
	}

	if (check_sum_cal % 0x100 != 0) {
		I("point data_checksum not match check_sum_cal: 0x%02X",
			check_sum_cal);
		ret_val = HX_CHKSUM_FAIL;
	} else if (zero_cnt == length) {
		if (ts->use_irq)
			I("[HIMAX TP MSG] All Zero event\n");

		ret_val = HX_CHKSUM_FAIL;
	} else {
		raw_data_sel = buf[HX_TOUCH_INFO_POINT_CNT]>>4 & 0x0F;
		if ((raw_data_sel != 0x0F)
		&& (raw_data_sel != hx_touch_data->diag_cmd)) {
			if (!hx_touch_data->diag_cmd) {
				g_core_fp.fp_read_event_stack(buf,
					(128-hx_touch_data->touch_info_size));
			}
			ret_val = HX_READY_SERVE;
		}
	}

END_FUNCTION:
	if (g_ts_dbg != 0)
		I("%s: END, ret_val=%d!\n", __func__, ret_val);
	return ret_val;
}

#if defined(HX_EXCP_RECOVERY)
#if defined(HW_ED_EXCP_EVENT)
static int himax_ts_event_check(struct himax_ts_data *ts,
		uint8_t *buf, int ts_path, int ts_status)
{
	uint32_t hx_EB_event = 0;
	uint32_t hx_EC_event = 0;
	uint32_t hx_EE_event = 0;
	uint32_t hx_ED_event = 0;
	uint32_t hx_excp_event = 0;
	int shaking_ret = 0;

	uint32_t i = 0;
	uint32_t length = 0;
	int ret_val = ts_status;

	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	switch (ts_path) {
	case HX_REPORT_COORD:
		length = hx_touch_data->touch_info_size;
		break;
#if defined(HX_SMART_WAKEUP)
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
		I("Now Path=%d, Now status=%d, length=%d\n",
				ts_path, ts_status, length);

	if (ts_path == HX_REPORT_COORD || ts_path == HX_REPORT_COORD_RAWDATA) {
		if (ic_data->HX_STYLUS_FUNC)
			length -= STYLUS_INFO_SZ;
		for (i = 0; i < length; i++) {
			if (buf[i] == 0xEB) {
				hx_EB_event++;
			} else if (buf[i] == 0xEC) {
				hx_EC_event++;
			} else if (buf[i] == 0xEE) {
				hx_EE_event++;
			} else if (buf[i] == 0xED) {
				hx_ED_event++;
			} else {
				g_zero_event_count = 0;
				break;
			}
		}
	}

	if (hx_EB_event == length) {
		hx_excp_event = length;
		hx_EB_event_flag++;
		I("[HIMAX TP MSG]: EXCEPTION event checked - ALL 0xEB.\n");
	} else if (hx_EC_event == length) {
		hx_excp_event = length;
		hx_EC_event_flag++;
		I("[HIMAX TP MSG]: EXCEPTION event checked - ALL 0xEC.\n");
	} else if (hx_EE_event == length) {
		hx_excp_event = length;
		hx_EE_event_flag++;
		I("[HIMAX TP MSG]: EXCEPTION event checked - ALL 0xEE.\n");
	} else if (hx_ED_event == length) {
		g_core_fp.fp_0f_reload_to_active();
	}

	if ((hx_excp_event == length || hx_ED_event == length)
		&& (HX_HW_RESET_ACTIVATE == 0)
		&& (HX_EXCP_RESET_ACTIVATE == 0)
		&& (hx_touch_data->diag_cmd == 0)) {
		shaking_ret = g_core_fp.fp_ic_excp_recovery(
			hx_excp_event, hx_ED_event, length);

		if (shaking_ret == HX_EXCP_EVENT) {
			g_core_fp.fp_read_FW_status();
			himax_excp_hw_reset();
			ret_val = HX_EXCP_EVENT;
		} else if (shaking_ret == HX_ZERO_EVENT_COUNT) {
			g_core_fp.fp_read_FW_status();
			ret_val = HX_ZERO_EVENT_COUNT;
		} else {
			I("IC is running. Nothing to be done!\n");
			ret_val = HX_IC_RUNNING;
		}

	} else if (HX_EXCP_RESET_ACTIVATE) {
		HX_EXCP_RESET_ACTIVATE = 0;
		I("%s: Skip by HX_EXCP_RESET_ACTIVATE.\n", __func__);
		ret_val = HX_EXCP_REC_OK;
	}

END_FUNCTION:
	if (g_ts_dbg != 0)
		I("%s: END, ret_val=%d!\n", __func__, ret_val);

	return ret_val;
}

#else
static int himax_ts_event_check(struct himax_ts_data *ts,
		uint8_t *buf, int ts_path, int ts_status)
{
	uint32_t hx_EB_event = 0;
	uint32_t hx_EC_event = 0;
	uint32_t hx_ED_event = 0;
	uint32_t hx_excp_event = 0;
	uint32_t hx_zero_event = 0;
	int shaking_ret = 0;

	uint32_t i = 0;
	uint32_t length = 0;
	int ret_val = ts_status;

	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	switch (ts_path) {
	case HX_REPORT_COORD:
		length = hx_touch_data->touch_info_size;
		break;
#if defined(HX_SMART_WAKEUP)
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
		I("Now Path=%d, Now status=%d, length=%d\n",
				ts_path, ts_status, length);

	if (ts_path == HX_REPORT_COORD || ts_path == HX_REPORT_COORD_RAWDATA) {
		if (ic_data->HX_STYLUS_FUNC)
			length -= STYLUS_INFO_SZ;
		for (i = 0; i < length; i++) {
			if (buf[i] == 0xEB) {
				hx_EB_event++;
			} else if (buf[i] == 0xEC) {
				hx_EC_event++;
			} else if (buf[i] == 0xED) {
				hx_ED_event++;
			} else if (buf[i] == 0x00) {
				hx_zero_event++;
			} else {
				g_zero_event_count = 0;
				break;
			}
		}
	}

	if (hx_EB_event == length) {
		hx_excp_event = length;
		hx_EB_event_flag++;
		I("[HIMAX TP MSG]: EXCEPTION event checked - ALL 0xEB.\n");
	} else if (hx_EC_event == length) {
		hx_excp_event = length;
		hx_EC_event_flag++;
		I("[HIMAX TP MSG]: EXCEPTION event checked - ALL 0xEC.\n");
	} else if (hx_ED_event == length) {
		hx_excp_event = length;
		hx_ED_event_flag++;
		I("[HIMAX TP MSG]: EXCEPTION event checked - ALL 0xED.\n");
	}

	if ((hx_excp_event == length || hx_zero_event == length)
		&& (HX_HW_RESET_ACTIVATE == 0)
		&& (HX_EXCP_RESET_ACTIVATE == 0)
		&& (hx_touch_data->diag_cmd == 0)) {
		shaking_ret = g_core_fp.fp_ic_excp_recovery(
			hx_excp_event, hx_zero_event, length);

		if (shaking_ret == HX_EXCP_EVENT) {
			g_core_fp.fp_read_FW_status();
			himax_excp_hw_reset();
			ret_val = HX_EXCP_EVENT;
		} else if (shaking_ret == HX_ZERO_EVENT_COUNT) {
			g_core_fp.fp_read_FW_status();
			ret_val = HX_ZERO_EVENT_COUNT;
		} else {
			I("IC is running. Nothing to be done!\n");
			ret_val = HX_IC_RUNNING;
		}

	} else if (HX_EXCP_RESET_ACTIVATE) {
		HX_EXCP_RESET_ACTIVATE = 0;
		I("%s: Skip by HX_EXCP_RESET_ACTIVATE.\n", __func__);
		ret_val = HX_EXCP_REC_OK;
	}

END_FUNCTION:
	if (g_ts_dbg != 0)
		I("%s: END, ret_val=%d!\n", __func__, ret_val);

	return ret_val;
}
#endif
#endif

static int himax_err_ctrl(struct himax_ts_data *ts,
		uint8_t *buf, int ts_path, int ts_status)
{
#if defined(HX_RST_PIN_FUNC)
	if (HX_HW_RESET_ACTIVATE) {
		HX_HW_RESET_ACTIVATE = 0;
		I("[HX_HW_RESET_ACTIVATE]%s:Back from reset,ready to serve.\n",
				__func__);
		ts_status = HX_RST_OK;
		goto END_FUNCTION;
	}
#endif

	ts_status = himax_checksum_cal(ts, buf, ts_path, ts_status);
	if (ts_status == HX_CHKSUM_FAIL) {
		goto CHK_FAIL;
	} else {
#if defined(HX_EXCP_RECOVERY)
		g_zero_event_count = 0;
#endif
		goto END_FUNCTION;
	}

CHK_FAIL:
#if defined(HX_EXCP_RECOVERY)
	ts_status = himax_ts_event_check(ts, buf, ts_path, ts_status);
#endif

END_FUNCTION:
	if (g_ts_dbg != 0)
		I("%s: END, ts_status=%d!\n", __func__, ts_status);
	return ts_status;
}

static int himax_distribute_touch_data(uint8_t *buf,
		int ts_path, int ts_status)
{
	uint8_t hx_state_info_pos = hx_touch_data->touch_info_size - 3;

	if (ic_data->HX_STYLUS_FUNC)
		hx_state_info_pos -= STYLUS_INFO_SZ;

	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	if (ts_path == HX_REPORT_COORD) {
		memcpy(hx_touch_data->hx_coord_buf, &buf[0],
				hx_touch_data->touch_info_size);

		if (buf[hx_state_info_pos] != 0xFF
		&& buf[hx_state_info_pos + 1] != 0xFF)
			memcpy(hx_touch_data->hx_state_info,
					&buf[hx_state_info_pos], 2);
		else
			memset(hx_touch_data->hx_state_info, 0x00,
					sizeof(hx_touch_data->hx_state_info));

		if ((HX_HW_RESET_ACTIVATE)
#if defined(HX_EXCP_RECOVERY)
		|| (HX_EXCP_RESET_ACTIVATE)
#endif
		) {
			memcpy(hx_touch_data->hx_rawdata_buf,
					&buf[hx_touch_data->touch_info_size],
					hx_touch_data->touch_all_size
					- hx_touch_data->touch_info_size);
		}
	} else if (ts_path == HX_REPORT_COORD_RAWDATA) {
		memcpy(hx_touch_data->hx_coord_buf, &buf[0],
				hx_touch_data->touch_info_size);

		if (buf[hx_state_info_pos] != 0xFF
		&& buf[hx_state_info_pos + 1] != 0xFF)
			memcpy(hx_touch_data->hx_state_info,
					&buf[hx_state_info_pos], 2);
		else
			memset(hx_touch_data->hx_state_info, 0x00,
					sizeof(hx_touch_data->hx_state_info));

memcpy(hx_touch_data->hx_rawdata_buf,
				&buf[hx_touch_data->touch_info_size],
				hx_touch_data->touch_all_size
				- hx_touch_data->touch_info_size);
#if defined(HX_SMART_WAKEUP)
	} else if (ts_path == HX_REPORT_SMWP_EVENT) {
		memcpy(hx_touch_data->hx_event_buf, buf,
				hx_touch_data->event_size);
#endif
	} else {
		E("%s, Fail Path!\n", __func__);
		ts_status = HX_PATH_FAIL;
	}

	if (g_ts_dbg != 0)
		I("%s: End, ts_status=%d!\n", __func__, ts_status);
	return ts_status;
}

#define READ_VAR_BIT(var, nb)			(((var) >> (nb)) & 0x1)

static bool wgp_pen_id_crc(uint8_t *p_id)
{
	uint64_t pen_id, input;
	uint8_t hash_id, devidend;
	uint8_t pol = 0x43;
	int i = 0;

	if (g_ts_dbg != 0) {
		for (i = 0; i < 8; i++)
			I("%s:pen id[%d]= %x\n", __func__,
				i, p_id[i]);
	}
	pen_id = (uint64_t)p_id[0] | ((uint64_t)p_id[1] << 8) |
		((uint64_t)p_id[2] << 16) | ((uint64_t)p_id[3] << 24) |
		((uint64_t)p_id[4] << 32) | ((uint64_t)p_id[5] << 40) |
		((uint64_t)p_id[6] << 48);
	hash_id = p_id[7];
	if (g_ts_dbg != 0)
		I("%s:pen id=%llx, hash id=%x\n", __func__,
			pen_id, hash_id);

	input = pen_id << 6;
	devidend = input >> (44 + 6);

	for (i = (44 + 6 - 1); i >= 0; i--)	{
		if (READ_VAR_BIT(devidend, 6))
			devidend = devidend ^ pol;
		devidend = devidend << 1 | READ_VAR_BIT(input, i);
	}
	if (READ_VAR_BIT(devidend, 6))
		devidend = devidend ^ pol;
	if (g_ts_dbg != 0)
		I("%s:devidend=%x\n", __func__, devidend);

	if (devidend == hash_id) {
		g_target_report_data->s[0].id = pen_id;
		return 1;
	}

	g_target_report_data->s[0].id = 0;
	return 0;
}

static	uint8_t p_id[8];

int himax_parse_report_points(struct himax_ts_data *ts,
		int ts_path, int ts_status)
{
	int x = 0, y = 0, w = 0;
	uint8_t p_hover = 0, p_btn = 0, p_btn2 = 0;
	uint8_t stylus_ratio = ic_data->HX_STYLUS_RATIO;
	uint8_t touchscreen_ratio = ic_data->HX_TOUCHSCREEN_RATIO;
	uint8_t p_id_en = 0, p_id_sel = 0;
	int8_t p_tilt_x = 0, p_tilt_y = 0;
	int p_x = 0, p_y = 0, p_w = 0;
	bool ret;
	static uint8_t p_p_on;

	int base = 0;
	int32_t	i = 0;

	if (g_ts_dbg != 0)
		I("%s: start!\n", __func__);

	base = hx_touch_data->touch_info_size;

	if (!ic_data->HX_STYLUS_FUNC)
		goto skip_stylus_operation;

	p_p_on = 0;
	base -= STYLUS_INFO_SZ;
	if (ic_data->HX_STYLUS_ID_V2) {
		p_x = hx_touch_data->hx_coord_buf[base] << 8
			| hx_touch_data->hx_coord_buf[base + 1];
		p_y = (hx_touch_data->hx_coord_buf[base + 2] << 8
			| hx_touch_data->hx_coord_buf[base + 3]);
		p_w = (hx_touch_data->hx_coord_buf[base + 4] << 8
			| hx_touch_data->hx_coord_buf[base + 5]);
		p_tilt_x = (int8_t)hx_touch_data->hx_coord_buf[base + 6];
		p_tilt_y = (int8_t)hx_touch_data->hx_coord_buf[base + 7];
		p_hover = hx_touch_data->hx_coord_buf[base + 8] & 0x01;
		p_btn = hx_touch_data->hx_coord_buf[base + 8] & 0x02;
		p_btn2 = hx_touch_data->hx_coord_buf[base + 8] & 0x04;
		p_id_en = hx_touch_data->hx_coord_buf[base + 8] & 0x08;
		if (!p_id_en) {
			g_target_report_data->s[0].battery_info =
				hx_touch_data->hx_coord_buf[base + 9];
			if (g_ts_dbg != 0) {
				I("%s:update battery info = %x\n", __func__,
				g_target_report_data->s[0].battery_info);
			}
		} else {
			p_id_sel =
			(hx_touch_data->hx_coord_buf[base + 8] & 0xF0) >> 4;
			p_id[p_id_sel*2] =
			hx_touch_data->hx_coord_buf[base + 9];
			p_id[p_id_sel*2 + 1] =
			hx_touch_data->hx_coord_buf[base + 10];
			if (g_ts_dbg != 0) {
				I("%s:update pen id, p_id_sel = %d\n", __func__,
				p_id_sel);
			}
			if (p_id_sel == 3) {
				ret = wgp_pen_id_crc(p_id);
				if (!ret)
					I("Pen_ID CRC not match\n");
			}
		}
	} else {
		p_x = hx_touch_data->hx_coord_buf[base] << 8
			| hx_touch_data->hx_coord_buf[base + 1];
		p_y = (hx_touch_data->hx_coord_buf[base + 2] << 8
			| hx_touch_data->hx_coord_buf[base + 3]);
		p_w = (hx_touch_data->hx_coord_buf[base + 4] << 8
			| hx_touch_data->hx_coord_buf[base + 5]);
		p_tilt_x = (int8_t)hx_touch_data->hx_coord_buf[base + 6];
		p_hover = hx_touch_data->hx_coord_buf[base + 7];
		p_btn = hx_touch_data->hx_coord_buf[base + 8];
		p_btn2 = hx_touch_data->hx_coord_buf[base + 9];
		p_tilt_y = (int8_t)hx_touch_data->hx_coord_buf[base + 10];
	}
	if (g_ts_dbg != 0) {
		D("%s: p_x=%d, p_y=%d, p_w=%d,p_tilt_x=%d, p_hover=%d\n",
			__func__, p_x, p_y, p_w, p_tilt_x, p_hover);
		D("%s: p_btn=%d, p_btn2=%d, p_tilt_y=%d\n",
			__func__, p_btn, p_btn2, p_tilt_y);
	}

	if (p_x >= 0
	&& p_x <= ((ts->pdata->abs_x_max+1)*stylus_ratio-1)
	&& p_y >= 0
	&& p_y <= ((ts->pdata->abs_y_max+1)*stylus_ratio-1)) {
		g_target_report_data->s[0].x = p_x;
		g_target_report_data->s[0].y = p_y;
		g_target_report_data->s[0].w = p_w;
		g_target_report_data->s[0].hover = p_hover;
		g_target_report_data->s[0].btn = p_btn;
		g_target_report_data->s[0].btn2 = p_btn2;
		g_target_report_data->s[0].tilt_x = p_tilt_x;
		g_target_report_data->s[0].tilt_y = p_tilt_y;
		g_target_report_data->s[0].on = 1;
		ts->hx_stylus_num++;
	} else {
		g_target_report_data->s[0].x = 0;
		g_target_report_data->s[0].y = 0;
		g_target_report_data->s[0].w = 0;
		g_target_report_data->s[0].hover = 0;
		g_target_report_data->s[0].btn = 0;
		g_target_report_data->s[0].btn2 = 0;
		g_target_report_data->s[0].tilt_x = 0;
		g_target_report_data->s[0].tilt_y = 0;
		g_target_report_data->s[0].on = 0;
	}

	if (g_ts_dbg != 0) {
		if (p_p_on != g_target_report_data->s[0].on) {
			I("s[0].on = %d, hx_stylus_num=%d\n",
				g_target_report_data->s[0].on,
				ts->hx_stylus_num);
			p_p_on = g_target_report_data->s[0].on;
		}
	}
skip_stylus_operation:

	ts->old_finger = ts->pre_finger_mask;
	if (ts->hx_point_num == 0) {
		if (g_ts_dbg != 0)
			I("%s: hx_point_num = 0!\n", __func__);
		return ts_status;
	}
	ts->pre_finger_mask = 0;
	hx_touch_data->finger_num =
		hx_touch_data->hx_coord_buf[base - 4] & 0x0F;
	hx_touch_data->finger_on = 1;
	AA_press = 1;

	g_target_report_data->finger_num = hx_touch_data->finger_num;
	g_target_report_data->finger_on = hx_touch_data->finger_on;
	g_target_report_data->ig_count =
		hx_touch_data->hx_coord_buf[base - 5];

	if (g_ts_dbg != 0)
		I("%s:finger_num = 0x%2X, finger_on = %d\n", __func__,
				g_target_report_data->finger_num,
				g_target_report_data->finger_on);

	for (i = 0; i < ts->nFinger_support; i++) {
		base = i * 4;
		x = hx_touch_data->hx_coord_buf[base] << 8
			| hx_touch_data->hx_coord_buf[base + 1];
		y = (hx_touch_data->hx_coord_buf[base + 2] << 8
			| hx_touch_data->hx_coord_buf[base + 3]);
		w = hx_touch_data->hx_coord_buf[(ts->nFinger_support * 4) + i];

		if (g_ts_dbg != 0)
			D("%s: now parsing[%d]:x=%d, y=%d, w=%d\n", __func__,
					i, x, y, w);

		if (x >= 0
		&& x <= ((ts->pdata->abs_x_max+1)*touchscreen_ratio-1)
		&& y >= 0
		&& y <= ((ts->pdata->abs_y_max+1)*touchscreen_ratio-1)) {
			hx_touch_data->finger_num--;

			g_target_report_data->p[i].x = x;
			g_target_report_data->p[i].y = y;
			g_target_report_data->p[i].w = w;
			g_target_report_data->p[i].id = 1;

			if (!ts->first_pressed) {
				ts->first_pressed = 1;
				I("S1@%d, %d\n", x, y);
			}

			ts->pre_finger_data[i][0] = x;
			ts->pre_finger_data[i][1] = y;

			ts->pre_finger_mask = ts->pre_finger_mask + (1<<i);
		} else {
			g_target_report_data->p[i].x = x;
			g_target_report_data->p[i].y = y;
			g_target_report_data->p[i].w = w;
			g_target_report_data->p[i].id = 0;

			if (i == 0 && ts->first_pressed == 1) {
				ts->first_pressed = 2;
				I("E1@%d, %d\n", ts->pre_finger_data[0][0],
						ts->pre_finger_data[0][1]);
			}
		}
	}

	if (g_ts_dbg != 0) {
		for (i = 0; i < ts->nFinger_support; i++)
			D("DBG X=%d  Y=%d ID=%d\n",
				g_target_report_data->p[i].x,
				g_target_report_data->p[i].y,
				g_target_report_data->p[i].id);

		D("DBG finger number %d\n", g_target_report_data->finger_num);
	}

	if (g_ts_dbg != 0)
		I("%s: end!\n", __func__);
	return ts_status;
}

static int himax_parse_report_data(struct himax_ts_data *ts,
		int ts_path, int ts_status)
{

	if (g_ts_dbg != 0)
		I("%s: start now_status=%d!\n", __func__, ts_status);


	EN_NoiseFilter =
		(hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT + 2] >> 3);
	EN_NoiseFilter = EN_NoiseFilter & 0x01;
	p_point_num = ts->hx_point_num;

	if (hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT] == 0xff)
		ts->hx_point_num = 0;
	else
		ts->hx_point_num =
			hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT]
			& 0x0f;

	if (ic_data->HX_STYLUS_FUNC) {
		p_stylus_num = ts->hx_stylus_num;
		ts->hx_stylus_num = 0;
	}

	switch (ts_path) {
	case HX_REPORT_COORD:
		ts_status = himax_parse_report_points(ts, ts_path, ts_status);
		break;
	case HX_REPORT_COORD_RAWDATA:
		if (debug_data != NULL) {
			if (debug_data->fp_set_diag_cmd(ic_data, hx_touch_data))
				I("%s:not match\n", __func__);
		} else {
			E("%s,There is no init set_diag_cmd\n", __func__);
		}
		ts_status = himax_parse_report_points(ts, ts_path, ts_status);
		break;
#if defined(HX_SMART_WAKEUP)
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

static void himax_report_all_leave_event(struct himax_ts_data *ts)
{
	int i = 0;

	for (i = 0; i < ts->nFinger_support; i++) {
#if !defined(HX_PROTOCOL_A)
		input_mt_slot(ts->input_dev, i);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
#endif
	}
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
	input_sync(ts->input_dev);
}

/* start report_point*/
static void himax_point_report(struct himax_ts_data *ts)
{
	int i = 0;
	bool valid = false;
	uint8_t ratio = ic_data->HX_TOUCHSCREEN_RATIO;

	if (g_ts_dbg != 0) {
		I("%s:start hx_touch_data->finger_num=%d\n",
			__func__, hx_touch_data->finger_num);
	}
	for (i = 0; i < ts->nFinger_support; i++) {
		if (g_target_report_data->p[i].x >= 0
		&& g_target_report_data->p[i].x <= ((ts->pdata->abs_x_max+1)*ratio-1)
		&& g_target_report_data->p[i].y >= 0
		&& g_target_report_data->p[i].y <= ((ts->pdata->abs_y_max+1)*ratio-1))
			valid = true;
		else
			valid = false;
		if (g_ts_dbg != 0)
			I("valid=%d\n", valid);
		if (valid) {
			if (g_ts_dbg != 0) {
				I("report_data->x[i]=%d,y[i]=%d,w[i]=%d",
					g_target_report_data->p[i].x,
					g_target_report_data->p[i].y,
					g_target_report_data->p[i].w);
			}
#if !defined(HX_PROTOCOL_A)
			input_mt_slot(ts->input_dev, i);
#else
			input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
					g_target_report_data->p[i].w);
#if !defined(HX_PROTOCOL_A)
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
					g_target_report_data->p[i].w);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
					g_target_report_data->p[i].w);
#else
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID,
					i + 1);
#endif
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
					g_target_report_data->p[i].x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
					g_target_report_data->p[i].y);
#if !defined(HX_PROTOCOL_A)
			ts->last_slot = i;
			input_mt_report_slot_state(ts->input_dev,
					MT_TOOL_FINGER, 1);
#else
			input_mt_sync(ts->input_dev);
#endif
		} else {
#if !defined(HX_PROTOCOL_A)
			input_mt_slot(ts->input_dev, i);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
			input_mt_report_slot_state(ts->input_dev,
					MT_TOOL_FINGER, 0);
#endif
		}
	}
#if !defined(HX_PROTOCOL_A)
	input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif
	input_sync(ts->input_dev);

	if (g_ts_dbg != 0)
		I("%s:end\n", __func__);
}

static void himax_point_leave(struct himax_ts_data *ts)
{
#if !defined(HX_PROTOCOL_A)
	int32_t i = 0;
#endif

	if (g_ts_dbg != 0)
		I("%s: start!\n", __func__);

	hx_touch_data->finger_on = 0;
	g_target_report_data->finger_on  = 0;
	g_target_report_data->finger_num = 0;
	AA_press = 0;

#if defined(HX_PROTOCOL_A)
	input_mt_sync(ts->input_dev);
#endif
#if !defined(HX_PROTOCOL_A)
	for (i = 0; i < ts->nFinger_support; i++) {
		input_mt_slot(ts->input_dev, i);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
	}
#endif
	if (ts->pre_finger_mask > 0)
		ts->pre_finger_mask = 0;

	if (ts->first_pressed == 1) {
		ts->first_pressed = 2;
		I("E1@%d, %d\n", ts->pre_finger_data[0][0],
				ts->pre_finger_data[0][1]);
	}

	input_report_key(ts->input_dev, BTN_TOUCH, 0);
	input_sync(ts->input_dev);

	if (g_ts_dbg != 0)
		I("%s: end!\n", __func__);
}

static void himax_stylus_report(struct himax_ts_data *ts)
{
	bool valid = false;
	uint8_t ratio = ic_data->HX_STYLUS_RATIO;

	if (g_ts_dbg != 0) {
		I("%s:start hx_touch_data->stylus_num=%d\n",
			__func__, ts->hx_stylus_num);
	}

	if (g_target_report_data->s[0].x >= 0
	&& g_target_report_data->s[0].x <= ((ts->pdata->abs_x_max+1)*ratio-1)
	&& g_target_report_data->s[0].y >= 0
	&& g_target_report_data->s[0].y <= ((ts->pdata->abs_y_max+1)*ratio-1)
	&& (g_target_report_data->s[0].on == 1))
		valid = true;
	else
		valid = false;

	if (g_ts_dbg != 0)
		I("stylus valid=%d\n", valid);

	if (valid) {/*stylus down*/
		if (g_ts_dbg != 0)
			I("s[i].x=%d, s[i].y=%d, s[i].w=%d\n",
					g_target_report_data->s[0].x,
					g_target_report_data->s[0].y,
					g_target_report_data->s[0].w);

		input_report_abs(ts->stylus_dev, ABS_X,
				g_target_report_data->s[0].x);
		input_report_abs(ts->stylus_dev, ABS_Y,
				g_target_report_data->s[0].y);

		if (g_target_report_data->s[0].btn !=
		g_target_report_data->s[0].pre_btn) {
			if (g_ts_dbg != 0)
				I("BTN_STYLUS:%d\n",
					g_target_report_data->s[0].btn);

			input_report_key(ts->stylus_dev, BTN_STYLUS,
					g_target_report_data->s[0].btn);

			g_target_report_data->s[0].pre_btn =
					g_target_report_data->s[0].btn;
		} else {
			if (g_ts_dbg != 0)
				I("BTN_STYLUS status is %d!\n",
						g_target_report_data->s[0].btn);
		}

		if (g_target_report_data->s[0].btn2
		!= g_target_report_data->s[0].pre_btn2) {
			if (g_ts_dbg != 0)
				I("BTN_STYLUS2:%d\n",
					g_target_report_data->s[0].btn2);

			input_report_key(ts->stylus_dev, BTN_STYLUS2,
					g_target_report_data->s[0].btn2);

			g_target_report_data->s[0].pre_btn2 =
					g_target_report_data->s[0].btn2;
		} else {
			if (g_ts_dbg != 0)
				I("BTN_STYLUS2 status is %d!\n",
					g_target_report_data->s[0].btn2);
		}
		input_report_abs(ts->stylus_dev, ABS_TILT_X,
				g_target_report_data->s[0].tilt_x);

		input_report_abs(ts->stylus_dev, ABS_TILT_Y,
				g_target_report_data->s[0].tilt_y);

		input_report_key(ts->stylus_dev, BTN_TOOL_PEN, 1);

		if (g_target_report_data->s[0].hover == 0) {
			input_report_key(ts->stylus_dev, BTN_TOUCH, 1);
			input_report_abs(ts->stylus_dev, ABS_DISTANCE, 0);
			input_report_abs(ts->stylus_dev, ABS_PRESSURE,
					g_target_report_data->s[0].w);
		} else {
			input_report_key(ts->stylus_dev, BTN_TOUCH, 0);
			input_report_abs(ts->stylus_dev, ABS_DISTANCE, 1);
			input_report_abs(ts->stylus_dev, ABS_PRESSURE, 0);
		}
	} else {/*Pen up*/
		g_target_report_data->s[0].pre_btn = 0;
		g_target_report_data->s[0].pre_btn2 = 0;
		input_report_key(ts->stylus_dev, BTN_STYLUS, 0);
		input_report_key(ts->stylus_dev, BTN_STYLUS2, 0);
		input_report_key(ts->stylus_dev, BTN_TOUCH, 0);
		input_report_abs(ts->stylus_dev, ABS_PRESSURE, 0);
		input_sync(ts->stylus_dev);

		input_report_abs(ts->stylus_dev, ABS_DISTANCE, 0);
		input_report_key(ts->stylus_dev, BTN_TOOL_RUBBER, 0);
		input_report_key(ts->stylus_dev, BTN_TOOL_PEN, 0);
		input_report_abs(ts->stylus_dev, ABS_PRESSURE, 0);
	}
	input_sync(ts->stylus_dev);

	if (g_ts_dbg != 0)
		I("%s:end\n", __func__);
}

static void himax_stylus_leave(struct himax_ts_data *ts)
{
	if (g_ts_dbg != 0)
		I("%s: start!\n", __func__);

	g_target_report_data->s[0].pre_btn = 0;
	g_target_report_data->s[0].pre_btn2 = 0;
	input_report_key(ts->stylus_dev, BTN_STYLUS, 0);
	input_report_key(ts->stylus_dev, BTN_STYLUS2, 0);
	input_report_key(ts->stylus_dev, BTN_TOUCH, 0);
	input_report_abs(ts->stylus_dev, ABS_PRESSURE, 0);
	input_sync(ts->stylus_dev);

	input_report_abs(ts->stylus_dev, ABS_DISTANCE, 0);
	input_report_abs(ts->stylus_dev, ABS_TILT_X, 0);
	input_report_abs(ts->stylus_dev, ABS_TILT_Y, 0);
	input_report_key(ts->stylus_dev, BTN_TOOL_RUBBER, 0);
	input_report_key(ts->stylus_dev, BTN_TOOL_PEN, 0);
	input_sync(ts->stylus_dev);

	if (g_ts_dbg != 0)
		I("%s: end!\n", __func__);
}

int himax_report_data(struct himax_ts_data *ts, int ts_path, int ts_status)
{
	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	if (ts_path == HX_REPORT_COORD || ts_path == HX_REPORT_COORD_RAWDATA) {
		/* Touch Point information */

		if (ts->hx_point_num != 0)
			himax_point_report(ts);
		else if (ts->hx_point_num == 0 && p_point_num != 0)
			himax_point_leave(ts);

		if (ic_data->HX_STYLUS_FUNC) {
			if (ts->hx_stylus_num != 0)
				himax_stylus_report(ts);
			else if (ts->hx_stylus_num == 0 && p_stylus_num != 0)
				himax_stylus_leave(ts);
		}

		Last_EN_NoiseFilter = EN_NoiseFilter;

#if defined(HX_SMART_WAKEUP)
	} else if (ts_path == HX_REPORT_SMWP_EVENT) {
		himax_wake_event_report();
#endif
	} else {
		E("%s:Fail Path!\n", __func__);
		ts_status = HX_PATH_FAIL;
	}

	if (g_ts_dbg != 0)
		I("%s: END, ts_status=%d!\n", __func__, ts_status);
	return ts_status;
}
/* end report_data */

static int himax_ts_operation(struct himax_ts_data *ts,
		int ts_path, int ts_status)
{
	uint8_t hw_reset_check[2];

	memset(ts->xfer_buff, 0x00, 128 * sizeof(uint8_t));
	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));

	ts_status = himax_touch_get(ts, ts->xfer_buff, ts_path, ts_status);
	if (ts_status == HX_TS_GET_DATA_FAIL)
		goto END_FUNCTION;

	ts_status = himax_distribute_touch_data(ts->xfer_buff,
			ts_path, ts_status);
	ts_status = himax_err_ctrl(ts, ts->xfer_buff, ts_path, ts_status);
	if (ts_status == HX_REPORT_DATA || ts_status == HX_TS_NORMAL_END)
		ts_status = himax_parse_report_data(ts, ts_path, ts_status);
	else
		goto END_FUNCTION;


	ts_status = himax_report_data(ts, ts_path, ts_status);


END_FUNCTION:
	return ts_status;
}

void himax_ts_work(struct himax_ts_data *ts)
{

	int ts_status = HX_TS_NORMAL_END;
	int ts_path = 0;

	if (debug_data != NULL) {
		if (debug_data->is_checking_irq) {
			if (g_ts_dbg != 0)
				I("Now checking IRQ, skip it!\n");
			return;
		}
		debug_data->fp_ts_dbg_func(ts, HX_FINGER_ON);
	}
	if (ts->notouch_frame > 0) {
		if (g_ts_dbg != 0)
			I("Skipit=%d\n", ts->notouch_frame--);
		else
			ts->notouch_frame--;
		return;
	}

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
	I("%s: Now reset the Touch chip.\n", __func__);
#if defined(HX_RST_PIN_FUNC)
	g_core_fp.fp_ic_reset(false, true);
#else
	g_core_fp.fp_system_reset();
#endif
#if defined(HX_ZERO_FLASH)
	if (g_core_fp.fp_0f_reload_to_active)
		g_core_fp.fp_0f_reload_to_active();
#endif
END_FUNCTION:
	if (debug_data != NULL)
		debug_data->fp_ts_dbg_func(ts, HX_FINGER_LEAVE);

}
/*end ts_work*/
enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer)
{
	struct himax_ts_data *ts;


	ts = container_of(timer, struct himax_ts_data, timer);
	queue_work(ts->himax_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

#if !defined(HX_ZERO_FLASH)
static int hx_chk_flash_sts(uint32_t size)
{
	int rslt = 0;


	rslt = (!g_core_fp.fp_calculateChecksum(false, size));
	/*avoid the FW is full of zero*/
	rslt |= g_core_fp.fp_flash_lastdata_check(size);

	return rslt;
}
#endif

static void himax_boot_upgrade(struct work_struct *work)
{
#if defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH)
	int fw_sts = -1;
#endif

#if defined(__EMBEDDED_FW__)
	g_embedded_fw.size = (size_t)_binary___Himax_firmware_bin_end -
			(size_t)_binary___Himax_firmware_bin_start;
#endif

#if defined(HX_ZERO_FLASH)
	g_boot_upgrade_flag = true;
#else
	if (hx_chk_flash_sts(ic_data->flash_size) == 1) {
		E("%s: check flash fail, please upgrade FW\n", __func__);
	#if defined(HX_BOOT_UPGRADE)
		g_boot_upgrade_flag = true;
	#else
		goto END;
	#endif
	} else {
		g_core_fp.fp_reload_disable(0);
		g_core_fp.fp_power_on_init();
		g_core_fp.fp_read_FW_ver();
		g_core_fp.fp_tp_info_check();
	}
#endif

#if defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH)
	fw_sts = i_get_FW();
	if (fw_sts < NO_ERR)
		goto END;

	g_core_fp.fp_bin_desc_get((unsigned char *)hxfw->data, HX1K);

#if defined(HX_BOOT_UPGRADE)
	if (g_boot_upgrade_flag == false) {
		if (himax_auto_update_check() == 0)
			g_boot_upgrade_flag = true;
	}
#endif

	if (g_boot_upgrade_flag == true) {
		if (i_update_FW() <= 0) {
			E("%s: Update FW fail\n", __func__);
		} else {
			I("%s: Update FW success\n", __func__);
		#if !defined(HX_ALG_OVERLAY)
			g_core_fp.fp_reload_disable(0);
			g_core_fp.fp_power_on_init();
		#endif
			g_core_fp.fp_read_FW_ver();
			g_core_fp.fp_tp_info_check();
		}
	}

	if (fw_sts == NO_ERR)
		release_firmware(hxfw);
	hxfw = NULL;
#endif

END:
	ic_boot_done = 1;
	himax_int_enable(1);
}


#if defined(HX_CONTAINER_SPEED_UP)
static void himax_resume_work_func(struct work_struct *work)
{
	himax_chip_common_resume(private_ts);
}

#endif
int hx_ic_register(void)
{
	int ret = !NO_ERR;

	I("%s:Entering!\n", __func__);

#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX852xJ)
	if (_hx852xJ_init()) {
		ret = NO_ERR;
		goto END;
	}
#endif
#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102)
	if (_hx83102_init()) {
		ret = NO_ERR;
		goto END;
	}
#endif
#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83108)
	if (_hx83108_init()) {
		ret = NO_ERR;
		goto END;
	}
#endif
#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112)
	if (_hx83112_init()) {
		ret = NO_ERR;
		goto END;
	}
#endif
#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83121)
	if (_hx83121_init()) {
		ret = NO_ERR;
		goto END;
	}
#endif

END:
	if (ret == NO_ERR)
		I("%s: detect IC!\n", __func__);
	else
		E("%s: There is no IC!\n", __func__);
	I("%s:END!\n", __func__);

	return ret;
}

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE

enum MODE_TYPE mode_type[9] = {
	Touch_Game_Mode,
	Touch_UP_THRESHOLD,
	Touch_Tolerance,
	Touch_Tap_Stability,
	Touch_Aim_Sensitivity,
	Touch_Edge_Filter,
	Touch_Panel_Orientation,
	Touch_Doubletap_Mode,
	Touch_Pen_ENABLE
};
static char himax_touch_vendor_read(void)
{
	char value = '4';
	I("%s Get touch vendor: %c\n", __func__, value);
	return value;
}

static uint8_t himax_panel_vendor_read(void)
{
	char value = '0';
	if (!private_ts)
		return value;

	return private_ts->lockdown_info[6];
}

static uint8_t himax_panel_color_read(void)
{
	char value = '0';
	if (!private_ts)
		return value;

	return private_ts->lockdown_info[2];
}

static uint8_t himax_panel_display_read(void)
{
	char value = '0';
	if (!private_ts)
		return value;

	return private_ts->lockdown_info[1];
}

static void himax_get_lockdown_info(struct work_struct *work)
{
	int ret = 0;

	ret = himax_lockdown_info_register_read(HX_LOCKDOWN_INFO_FLASH_SADDR,private_ts->lockdown_info,DATA_LEN_8);
	if (ret < 0) {
		E("can't get lockdown info");
	} else {
		I("Lockdown:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
		private_ts->lockdown_info[0], private_ts->lockdown_info[1], private_ts->lockdown_info[2], private_ts->lockdown_info[3],
		private_ts->lockdown_info[4], private_ts->lockdown_info[5], private_ts->lockdown_info[6], private_ts->lockdown_info[7]);
	}
	I("READ LOCKDOWN!!!");
}

static int himax_set_cur_value(int himax_mode, int himax_value)
{
	if (himax_mode >= Touch_Mode_NUM && himax_mode < 0) {
		E("%s, himax mode is error:%d", __func__, himax_mode);
		return -EINVAL;
	}

	I("mode:%d, value:%d", himax_mode, himax_value);
	xiaomi_touch_interfaces.touch_mode[himax_mode][SET_CUR_VALUE] = himax_value;

	if (himax_mode == Touch_Expert_Mode) {
		I("This is Expert Mode, mode is %d", himax_value);
		xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] =
			private_ts->gamemode_config[himax_value - 1][0];
		xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] =
			private_ts->gamemode_config[himax_value - 1][1];
		xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][SET_CUR_VALUE] =
			private_ts->gamemode_config[himax_value - 1][2];
		xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][SET_CUR_VALUE] =
			private_ts->gamemode_config[himax_value - 1][3];
		xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] =
			private_ts->gamemode_config[himax_value - 1][4];
	}

	if (bTouchIsAwake) {
		queue_work(private_ts->set_touchfeature_wq, &private_ts->set_touchfeature_work);
	}

	return 0;
}

static void himax_init_touchmode_data(void)
{
	int i;

	/* Touch Game Mode Switch */
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] = 0;

	/* Tap Sensivity */
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = 0;

	/* Touch Tolerance */
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = 0;

	/* Touch Aim Sensitivity */
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_DEF_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_CUR_VALUE] = 0;

	/* Touch Tap Stability */
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_CUR_VALUE] = 0;

	/* Touch Expert Mode */
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_MIN_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_DEF_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][SET_CUR_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_CUR_VALUE] = 1;

	/* Edge filter area*/
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 0;

	/* Panel orientation*/
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] = 0;

	/*gesture mode*/
	xiaomi_touch_interfaces.touch_mode[Touch_Doubletap_Mode][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Doubletap_Mode][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Doubletap_Mode][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Doubletap_Mode][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Doubletap_Mode][GET_CUR_VALUE] = 0;

	/*pen mode*/
	xiaomi_touch_interfaces.touch_mode[Touch_Pen_ENABLE][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Pen_ENABLE][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Pen_ENABLE][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Pen_ENABLE][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Pen_ENABLE][GET_CUR_VALUE] = 0;

	for (i = 0; i < Touch_Mode_NUM; i++) {
		I("mode:%d, set cur:%d, get cur:%d, def:%d min:%d max:%d\n",
			i,
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MIN_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MAX_VALUE]);
	}

	return;
}

static void update_touchfeature_value_work(struct work_struct *work) {
	int ret = -1;
	uint8_t i = 0;
	uint8_t temp_set_value;
	uint8_t temp_get_value;
	uint8_t himax_game_value[2] = {0};

	int size = sizeof(mode_type)/sizeof(mode_type[0]);

	I("enter fp_game_mode_set\n");
	for (i = 0; i < size; i++) {
		temp_get_value = xiaomi_touch_interfaces.touch_mode[mode_type[i]][GET_CUR_VALUE];
		temp_set_value = xiaomi_touch_interfaces.touch_mode[mode_type[i]][SET_CUR_VALUE];

		if (temp_get_value == temp_set_value)
		continue;

		himax_game_value[0] = mode_type[i];
		himax_game_value[1] = temp_set_value;
		ret = g_core_fp.fp_game_mode_set(himax_game_value);
		msleep(40);
		if (ret < 0) {
			E("change game mode fail, mode is %d\n", mode_type[i]);
			return;
		}
		xiaomi_touch_interfaces.touch_mode[mode_type[i]][GET_CUR_VALUE] = temp_set_value;
		I("set mode:%d = %d\n", mode_type[i], temp_set_value);
	}

	I("exit fp_game_mode_set\n");
}

static int himax_get_mode_value(int mode, int value_type)
{
	int value = -1;

	if (mode < Touch_Mode_NUM && mode >= 0)
		value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
	else
		E("%s, don't support\n", __func__);

	return value;
}

static int himax_reset_mode(int mode)
{
	int i = 0;

	if (mode < Touch_Report_Rate && mode > 0) {
		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		himax_set_cur_value(mode, xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE]);
	} else if (mode == 0) {
		for (i = 0; i <= Touch_Report_Rate; i++) {
			if (i == Touch_Panel_Orientation) {
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
					xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE];
			} else {
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
					xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE];
			}
			if (i == Touch_Expert_Mode) {
				continue;
			}
			himax_set_cur_value(i, xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE]);
		}
	} else {
		E("%s, don't support\n",  __func__);
	}

	I("%s, mode:%d\n",  __func__, mode);
	return 0;
}

static int himax_get_mode_all(int mode, int *value)
{
	if (mode < Touch_Mode_NUM && mode >= 0) {
		value[0] = xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE];
		value[1] = xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		value[2] = xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
		value[3] = xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];
	} else {
		E("%s, don't support\n",  __func__);
	}
	I("%s, mode:%d, value:%d:%d:%d:%d\n", __func__, mode, value[0],
					value[1], value[2], value[3]);

	return 0;
}

static void himax_tp_mode_recovery(void)
{
	int ret = -1;
	uint8_t i = 0;
	uint8_t temp_set_value;
	uint8_t himax_game_value[2] = {0};
	int size = sizeof(mode_type)/sizeof(mode_type[0]);

	for (i = 0; i < size; i++) {
		temp_set_value = xiaomi_touch_interfaces.touch_mode[mode_type[i]][SET_CUR_VALUE];
		himax_game_value[0] = mode_type[i];
		himax_game_value[1] = temp_set_value;
		ret = g_core_fp.fp_game_mode_set(himax_game_value);
		msleep(17);
		if (ret < 0) {
			E("tp mode recovery fail, mode is %d\n", mode_type[i]);
			return;
		}
		xiaomi_touch_interfaces.touch_mode[mode_type[i]][GET_CUR_VALUE] = temp_set_value;
		I("recovery mode:%d = %d\n", mode_type[i], temp_set_value);
	}

	I("exit %s\n",__func__);
}

static void himax_reg_init()
{
	int ret = -1;
	uint8_t i = 0;
	uint8_t temp_set_value;
	uint8_t reset_mode_type[2] = {Touch_Game_Mode,Touch_Pen_ENABLE};
	uint8_t himax_game_value[2] = {0};
	int size = sizeof(reset_mode_type);

	for (i = 0; i < size; i++) {
		temp_set_value = xiaomi_touch_interfaces.touch_mode[reset_mode_type[i]][GET_DEF_VALUE];
		himax_game_value[0] = reset_mode_type[i];
		himax_game_value[1] = temp_set_value;
		ret = g_core_fp.fp_game_mode_set(himax_game_value);
		msleep(17);
		if (ret < 0) {
			E("reg init fail, mode is %d\n", reset_mode_type[i]);
			return;
		}
		xiaomi_touch_interfaces.touch_mode[reset_mode_type[i]][GET_CUR_VALUE] = temp_set_value;
		I("reg init mode:%d = %d\n", reset_mode_type[i], temp_set_value);
	}

	I("exit %s\n",__func__);
}
#endif
extern int himax_common_resume(struct device *dev);

static void himax_resume_work(struct work_struct *work)
{
	struct himax_ts_data *ts_core = container_of(work, struct himax_ts_data, resume_work);
	himax_common_resume(&ts_core->spi->dev);
}

int himax_chip_common_init(void)
{
	int ret = 0;
	int err = PROBE_FAIL;
	struct himax_ts_data *ts = private_ts;
	struct himax_platform_data *pdata;

	I("Prepare kernel fp\n");
	kp_getname_kernel = (void *)kallsyms_lookup_name("getname_kernel");
	if (!kp_getname_kernel) {
		E("prepare kp_getname_kernel failed!\n");
		/*goto err_xfer_buff_fail;*/
	}

	kp_putname_kernel = (void *)kallsyms_lookup_name("putname");
	if (!kp_putname_kernel) {
		E("prepare kp_putname_kernel failed!\n");
		/*goto err_xfer_buff_fail;*/
	}

	kp_file_open_name = (void *)kallsyms_lookup_name("file_open_name");
	if (!kp_file_open_name) {
		E("prepare kp_file_open_name failed!\n");
		goto err_xfer_buff_fail;
	}

	ts->xfer_buff = devm_kzalloc(ts->dev, 128 * sizeof(uint8_t),
			GFP_KERNEL);
	if (ts->xfer_buff == NULL) {
		err = -ENOMEM;
		goto err_xfer_buff_fail;
	}

	I("PDATA START\n");
	pdata = kzalloc(sizeof(struct himax_platform_data), GFP_KERNEL);

	if (pdata == NULL) { /*Allocate Platform data space*/
		err = -ENOMEM;
		goto err_dt_platform_data_fail;
	}

	I("ic_data START\n");
	ic_data = kzalloc(sizeof(struct himax_ic_data), GFP_KERNEL);
	if (ic_data == NULL) { /*Allocate IC data space*/
		err = -ENOMEM;
		goto err_dt_ic_data_fail;
	}
	memset(ic_data, 0xFF, sizeof(struct himax_ic_data));
	/* default 128k, different size please follow HX83121A style */
	ic_data->flash_size = FW_SIZE_255k;

	/* allocate report data */
	hx_touch_data = kzalloc(sizeof(struct himax_report_data), GFP_KERNEL);
	if (hx_touch_data == NULL) {
		err = -ENOMEM;
		goto err_alloc_touch_data_failed;
	}

	ts->pdata = pdata;

	if (himax_parse_dt(ts, pdata) < 0) {
		I(" pdata is NULL for DT\n");
		goto err_alloc_dt_pdata_failed;
	}

	if (pdata->virtual_key)
		ts->button = pdata->virtual_key;

#if defined(HX_RST_PIN_FUNC)
	ts->rst_gpio = pdata->gpio_reset;
#endif

	himax_gpio_power_config(pdata);

#if !defined(CONFIG_OF)
	if (pdata->power) {
		ret = pdata->power(1);
		if (ret < 0) {
			E("%s: power on failed\n", __func__);
			goto err_power_failed;
		}
	}
#endif

#if defined(CONFIG_OF)
	ts->power = pdata->power;
#endif

	g_hx_chip_inited = 0;

	if (hx_ic_register() != NO_ERR) {
		E("%s: can't detect IC!\n", __func__);
		goto error_ic_detect_failed;
	}

	private_ts->notouch_frame = 0;
	private_ts->ic_notouch_frame = 0;

	if (g_core_fp.fp_chip_init != NULL) {
		g_core_fp.fp_chip_init();
	} else {
		E("%s: function point of chip_init is NULL!\n", __func__);
		goto error_ic_detect_failed;
	}
#ifdef HX_PARSE_FROM_DT
	himax_parse_dt_ic_info(ts, pdata);
#endif
	g_core_fp.fp_touch_information();

	spin_lock_init(&ts->irq_lock);

	if (himax_ts_register_interrupt()) {
		E("%s: register interrupt failed\n", __func__);
		goto err_register_interrupt_failed;
	}

	himax_int_enable(0);

	/*lockdown_info*/
	ts->himax_lockdown_wq = create_singlethread_workqueue("himax_lockdown_wq");
	if (!ts->himax_lockdown_wq) {
		E("himax_fwu_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_himax_lockdown_wq_failed;
	}
	else{
		I("himax_fwu_wq create workqueue successful!\n");
	}
	INIT_WORK(&ts->himax_lockdown_work, himax_get_lockdown_info);
	// please make sure boot update start after display reset(RESX) sequence
	queue_work(ts->himax_lockdown_wq, &ts->himax_lockdown_work);

	himax_init_touchmode_data();
	himax_reg_init();
	/*update firmware*/
	ts->himax_boot_upgrade_wq =
		create_singlethread_workqueue("HX_boot_upgrade");
	if (!ts->himax_boot_upgrade_wq) {
		E("allocate himax_boot_upgrade_wq failed\n");
		err = -ENOMEM;
		goto err_boot_upgrade_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->work_boot_upgrade, himax_boot_upgrade);
	queue_delayed_work(ts->himax_boot_upgrade_wq, &ts->work_boot_upgrade,
			msecs_to_jiffies(200));

	g_core_fp.fp_calc_touch_data_size();


#if defined(CONFIG_OF)
	ts->pdata->abs_pressure_min        = 0;
	ts->pdata->abs_pressure_max        = 200;
	ts->pdata->abs_width_min           = 0;
	ts->pdata->abs_width_max           = 200;
	pdata->cable_config[0]             = 0xF0;
	pdata->cable_config[1]             = 0x00;
#endif

	ts->suspended                      = false;

#if defined(HX_PROTOCOL_A)
	ts->protocol_type = PROTOCOL_TYPE_A;
#else
	ts->protocol_type = PROTOCOL_TYPE_B;
#endif
	I("%s: Use Protocol Type %c\n", __func__,
		ts->protocol_type == PROTOCOL_TYPE_A ? 'A' : 'B');

#if defined(HX_SMART_WAKEUP)
	ts->SMWP_enable = 0;
#if defined(KERNEL_VER_ABOVE_4_19)
	ts->ts_SMWP_wake_lock =
		wakeup_source_register(ts->dev, HIMAX_common_NAME);
#else
	if (!ts->ts_SMWP_wake_lock)
		ts->ts_SMWP_wake_lock = kzalloc(sizeof(struct wakeup_source),
			GFP_KERNEL);

	if (!ts->ts_SMWP_wake_lock) {
		E("%s: allocate ts_SMWP_wake_lock failed\n", __func__);
		goto err_smwp_wake_lock_failed;
	}

	wakeup_source_init(ts->ts_SMWP_wake_lock, HIMAX_common_NAME);
#endif
#endif

#if defined(HX_HIGH_SENSE)
	ts->HSEN_enable = 0;
#endif

	if (himax_common_proc_init()) {
		E(" %s: himax_common proc_init failed!\n", __func__);
		goto err_creat_proc_file_failed;
	}

	if(sysfs_node_init()<0){
		E(" %s: crate sysfs node failed!\n", __func__);
		goto err_creat_sysfs_node_failed;
	}
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	if (himax_debug_init()) {
		E(" %s: debug initial failed!\n", __func__);
		goto err_debug_init_failed;
	}
#endif

	ret = himax_input_register(ts);
	if (ret) {
		E("%s: Unable to register %s input device\n",
			__func__, ts->input_dev->name);
		goto err_input_register_device_failed;
	}

#if defined(HX_CONTAINER_SPEED_UP)
	ts->ts_int_workqueue =
			create_singlethread_workqueue("himax_ts_resume_wq");
	if (!ts->ts_int_workqueue) {
		E("create ts_resume workqueue failed\n");
		goto err_create_ts_resume_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->ts_int_work, himax_resume_work_func);
#endif

	ts->initialized = true;

#ifdef HX_CONFIG_DRM
	ts->drm_notif.notifier_call = drm_notifier_callback;
	ret = mi_drm_register_client(&ts->drm_notif);
	if(ret){
		E("register drm_notifier failed. ret=%d\n", ret);
		goto err_register_drm_notif_failed;
	}
#else
	ts->fb_notif.notifier_call = fp_notifier_callback;
	ret = mi_drm_register_client(&ts->fb_notif);
	if(ret) {
		E("register fb_notifier failed. ret=%d\n", ret);
		goto err_register_fb_notif_failed;
	}
#endif

	/*touchfeature*/
	private_ts->set_touchfeature_wq = create_singlethread_workqueue("himax-set-touchfeature-queue");
	if (!private_ts->set_touchfeature_wq) {
		E("create set touch feature workqueue fail");
		ret = -ENOMEM;
		goto err_create_set_touchfeature_work_queue;
	}
	INIT_WORK(&private_ts->set_touchfeature_work, update_touchfeature_value_work);

	/*resume*/
	private_ts->event_wq = alloc_workqueue("himax-event-queue",
		WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!private_ts->event_wq) {
		E("Can not create work thread for resume!!");
		ret = -ENOMEM;
		goto err_alloc_work_thread_failed;
	}
	INIT_WORK(&private_ts->resume_work, himax_resume_work);

	/* is usb exit init */
#if defined(HX_USB_DETECT_GLOBAL)
	private_ts->usb_connected = 0x00;
	private_ts->cable_config = pdata->cable_config;

	private_ts->usb_exist= -1;
	mutex_init(&private_ts->power_supply_lock);
	INIT_WORK(&private_ts->himax_supply_work, himax_power_supply_work);
	private_ts->power_supply_notifier.notifier_call = himax_power_supply_event;
	ret = power_supply_reg_notifier(&private_ts->power_supply_notifier);
	if (ret) {
		E("register power_supply_notifier failed. ret=%d\n", ret);
		goto err_register_power_supply_notif_failed;
	}
#endif

#if defined(HX_PEN_DETECT_GLOBAL)
	ts->pen_is_charge = 0;
	ts->pen_exist= -1;
	mutex_init(&ts->pen_supply_lock);
	INIT_WORK(&ts->pen_supply_change_work, himax_pen_supply_work);
	ts->pen_supply_notifier.notifier_call = himax_pen_supply_notifier_callback;
	ret = pen_charge_state_notifier_register_client(&ts->pen_supply_notifier);
	if (ret) {
		E("register pen_supply_notifier failed. ret=%d\n", ret);
		goto err_register_pen_supply_notif_failed;
	}
#endif


#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
	xiaomi_touch_interfaces.touch_vendor_read = himax_touch_vendor_read;
	xiaomi_touch_interfaces.panel_display_read = himax_panel_display_read;
	xiaomi_touch_interfaces.panel_vendor_read = himax_panel_vendor_read;
	xiaomi_touch_interfaces.panel_color_read = himax_panel_color_read;
	xiaomi_touch_interfaces.setModeValue = himax_set_cur_value;
	xiaomi_touch_interfaces.getModeValue = himax_get_mode_value;
	xiaomi_touch_interfaces.resetMode = himax_reset_mode;
	xiaomi_touch_interfaces.getModeAll = himax_get_mode_all;
	xiaomitouch_register_modedata(&xiaomi_touch_interfaces);
#endif
	ts->db_wakeup = 0;
	private_ts->gesture_command_delayed = -1;
	bTouchIsAwake = 1;
	g_hx_chip_inited = true;
	return 0;

	pen_charge_state_notifier_unregister_client(&ts->pen_supply_notifier);
err_register_pen_supply_notif_failed:
	mutex_destroy(&ts->pen_supply_lock);
	power_supply_unreg_notifier(&private_ts->power_supply_notifier);
err_register_power_supply_notif_failed:
	mutex_destroy(&ts->power_supply_lock);
	destroy_workqueue(private_ts->event_wq);
err_alloc_work_thread_failed:
	destroy_workqueue(ts->set_touchfeature_wq);
err_create_set_touchfeature_work_queue:
#ifdef HX_CONFIG_DRM
	if (mi_drm_unregister_client(&ts->drm_notif))
		E("Error occurred while unregistering drm_notifier.\n");
err_register_drm_notif_failed:
#else
	if (mi_drm_unregister_client(&ts->fb_notif))
		E("Error occurred while unregistering fb_notifier.\n");
err_register_fb_notif_failed:
#endif

#if defined(HX_CONTAINER_SPEED_UP)
	cancel_delayed_work_sync(&ts->ts_int_work);
	destroy_workqueue(ts->ts_int_workqueue);
err_create_ts_resume_wq_failed:
#endif
	input_unregister_device(ts->input_dev);
	if (ic_data->HX_STYLUS_FUNC)
		input_unregister_device(ts->stylus_dev);
err_input_register_device_failed:
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_debug_remove();
err_debug_init_failed:
#endif

#if defined(HX_PEN_CONNECT_STRATEGY)
	class_destroy(sys_class);
err_creat_sysfs_node_failed:
#endif

	himax_common_proc_deinit();
err_creat_proc_file_failed:
#if defined(HX_SMART_WAKEUP)
#if defined(KERNEL_VER_ABOVE_4_19)
	wakeup_source_unregister(ts->ts_SMWP_wake_lock);
#else
	wakeup_source_trash(ts->ts_SMWP_wake_lock);
	kfree(ts->ts_SMWP_wake_lock);
	ts->ts_SMWP_wake_lock = NULL;
err_smwp_wake_lock_failed:
#endif
#endif
	cancel_delayed_work_sync(&ts->work_boot_upgrade);
	destroy_workqueue(ts->himax_boot_upgrade_wq);
err_boot_upgrade_wq_failed:
	if (ts->himax_lockdown_wq) {
		cancel_work_sync(&ts->himax_lockdown_work);
		destroy_workqueue(ts->himax_lockdown_wq);
		ts->himax_lockdown_wq = NULL;
	}
err_create_himax_lockdown_wq_failed:
	himax_ts_unregister_interrupt();
err_register_interrupt_failed:
/*err_detect_failed:*/
error_ic_detect_failed:
#if !defined(CONFIG_OF)
err_power_failed:
#endif
	himax_gpio_power_deconfig(pdata);
err_alloc_dt_pdata_failed:
	kfree(hx_touch_data);
	hx_touch_data = NULL;
err_alloc_touch_data_failed:
	kfree(ic_data);
	ic_data = NULL;
err_dt_ic_data_fail:
	kfree(pdata);
	pdata = NULL;
err_dt_platform_data_fail:
	devm_kfree(ts->dev, ts->xfer_buff);
	ts->xfer_buff = NULL;
err_xfer_buff_fail:
	probe_fail_flag = 1;
	return err;
}

void himax_chip_common_deinit(void)
{
	struct himax_ts_data *ts = private_ts;

	himax_ts_unregister_interrupt();

#if defined(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)
	himax_inspect_data_clear();
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_debug_remove();
#endif

	himax_common_proc_deinit();
	himax_report_data_deinit();

#if defined(HX_SMART_WAKEUP)
#if defined(KERNEL_VER_ABOVE_4_19)
	wakeup_source_unregister(ts->ts_SMWP_wake_lock);
#else
	wakeup_source_trash(ts->ts_SMWP_wake_lock);
	kfree(ts->ts_SMWP_wake_lock);
	ts->ts_SMWP_wake_lock = NULL;
#endif
#endif
#if defined(HX_CONFIG_FB)
	if (fb_unregister_client(&ts->fb_notif))
		E("Error occurred while unregistering fb_notifier.\n");
	cancel_delayed_work_sync(&ts->work_att);
	destroy_workqueue(ts->himax_att_wq);
#elif defined(HX_CONFIG_DRM_PANEL)
if (active_panel)
	if (drm_panel_notifier_unregister(active_panel,
		&ts->fb_notif))
		E("Error occurred while unregistering active_panel.\n");
else
	if (drm_panel_notifier_unregister(&gNotifier_dummy_panel,
		&ts->fb_notif))
		E("Error occurred while unregistering dummy_panel.\n");
	cancel_delayed_work_sync(&ts->work_att);
	destroy_workqueue(ts->himax_att_wq);
#elif defined(HX_CONFIG_DRM)
#if defined(__HIMAX_MOD__)
	hx_msm_drm_unregister_client =
		(void *)kallsyms_lookup_name("msm_drm_unregister_client");
	if (hx_msm_drm_unregister_client != NULL) {
		if (hx_msm_drm_unregister_client(&ts->fb_notif))
			E("Error occurred while unregistering drm_notifier.\n");
	} else
		E("hx_msm_drm_unregister_client is NULL\n");
#else
	if (mi_drm_unregister_client(&ts->drm_notif))
		E("Error occurred while unregistering drm_notifier.\n");
#endif
	cancel_delayed_work_sync(&ts->work_att);
	destroy_workqueue(ts->himax_att_wq);
#endif
	input_free_device(ts->input_dev);
#if defined(HX_CONTAINER_SPEED_UP)
	cancel_delayed_work_sync(&ts->ts_int_work);
	destroy_workqueue(ts->ts_int_workqueue);
#endif

#if defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH)
	cancel_delayed_work_sync(&ts->work_boot_upgrade);
	destroy_workqueue(ts->himax_boot_upgrade_wq);
#endif
	himax_gpio_power_deconfig(ts->pdata);
	if (himax_mcu_cmd_struct_free)
		himax_mcu_cmd_struct_free();

	kfree(hx_touch_data);
	hx_touch_data = NULL;
	kfree(ic_data);
	ic_data = NULL;
	kfree(ts->pdata->virtual_key);
	ts->pdata->virtual_key = NULL;
	devm_kfree(ts->dev, ts->xfer_buff);
	ts->xfer_buff = NULL;
	kfree(ts->pdata);
	ts->pdata = NULL;
	kfree(ts);
	ts = NULL;
	probe_fail_flag = 0;
#if defined(HX_USE_KSYM)
	hx_release_chip_entry();
#endif

	I("%s: Common section deinited!\n", __func__);
}

int himax_chip_common_suspend(struct himax_ts_data *ts)
{
	if (ts->suspended) {
		I("%s: Already suspended, skip...\n", __func__);
		goto END;
	} else {
		ts->suspended = true;
	}

	if (debug_data != NULL && debug_data->flash_dump_going == true) {
		I("%s: It is dumping flash, reject suspend\n", __func__);
		goto END;
	}


	if (ts->in_self_test == 1) {
		atomic_set(&ts->suspend_mode, 1);
		ts->pre_finger_mask = 0;
		if (g_core_fp._ap_notify_fw_sus != NULL)
			g_core_fp._ap_notify_fw_sus(1);
		ts->suspend_resume_done = 1;
		goto END;
	}

	g_core_fp.fp_suspend_proc(ts->suspended);

	if (ts->SMWP_enable) {
		if (g_core_fp.fp_0f_overlay != NULL)
			g_core_fp.fp_0f_overlay(2, 0);

		if (g_core_fp._ap_notify_fw_sus != NULL)
			g_core_fp._ap_notify_fw_sus(1);
		atomic_set(&ts->suspend_mode, 1);
		ts->pre_finger_mask = 0;
		I("%s: SMART WAKE UP enable, reject suspend\n", __func__);
		goto END;
	}

	himax_int_enable(0);
	himax_report_all_leave_event(ts);
	if (g_core_fp.fp_suspend_ic_action != NULL)
		g_core_fp.fp_suspend_ic_action();

	if (!ts->use_irq) {
		int32_t cancel_state;

		cancel_state = cancel_work_sync(&ts->work);
		if (cancel_state)
			himax_int_enable(1);
	}

	/*ts->first_pressed = 0;*/
	atomic_set(&ts->suspend_mode, 1);
	ts->pre_finger_mask = 0;

	if (ts->pdata)
		if (ts->pdata->powerOff3V3 && ts->pdata->power)
			ts->pdata->power(0);

END:
	bTouchIsAwake = 0;
	I("%s: END\n", __func__);

	return 0;
}

int himax_chip_common_resume(struct himax_ts_data *ts)
{
	if (ts->suspended == false) {
		I("%s: Already resumed, skip...\n", __func__);
		goto END;
	} else {
		ts->suspended = false;
	}

	if (ts->in_self_test == 1) {
		atomic_set(&ts->suspend_mode, 0);
		ts->diag_cmd = 0;
		if (g_core_fp._ap_notify_fw_sus != NULL)
			g_core_fp._ap_notify_fw_sus(0);
		ts->suspend_resume_done = 1;
		goto END;
	}

#if defined(HX_EXCP_RECOVERY)
	/* continuous N times record, not total N times. */
	g_zero_event_count = 0;
#endif

	atomic_set(&ts->suspend_mode, 0);
	ts->diag_cmd = 0;

	if (ts->pdata)
		if (ts->pdata->powerOff3V3 && ts->pdata->power)
			ts->pdata->power(1);
#if defined(HX_RST_PIN_FUNC) && defined(HX_RESUME_HW_RESET)
	if (g_core_fp.fp_ic_reset != NULL)
		g_core_fp.fp_ic_reset(false, false);
#endif

	g_core_fp.fp_resume_proc(ts->suspended);
	himax_report_all_leave_event(ts);
	himax_int_enable(1);

/*tp anti usb interference*/
private_ts->usb_exist = -1;
queue_work(private_ts ->event_wq, &private_ts ->himax_supply_work);

/*tp anti pen charge interference*/
private_ts->pen_exist = -1;
schedule_work(&private_ts->pen_supply_change_work);

/*gesture mode*/
if(private_ts->gesture_command_delayed >=0){
	private_ts->db_wakeup = private_ts->gesture_command_delayed;
	private_ts->gesture_command_delayed = -1;
	I("execute delayed command, set double click wakeup %d\n", private_ts->db_wakeup);
	dsi_panel_doubleclick_enable(!!ts->db_wakeup);
}

bTouchIsAwake = 1;

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
	I("reload the tp mode cmd");
	himax_tp_mode_recovery();
#endif
END:
	I("%s: END\n", __func__);
	return 0;
}
