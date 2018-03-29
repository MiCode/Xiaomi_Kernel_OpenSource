/***************************************************************************
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *    File	: lgtp_device_dummy.c
 *    Author(s)   : D3 BSP Touch Team < d3-bsp-touch@lge.com >
 *    Description :
 *
 ***************************************************************************/
#define LGTP_MODULE "[FT8707]"

/****************************************************************************
* Include Files
****************************************************************************/
#include <lgtp_common.h>

#include <lgtp_common_driver.h>
#include <lgtp_platform_api_i2c.h>
#include <lgtp_platform_api_misc.h>
#include <lgtp_device_ft8707.h>


/****************************************************************************
* Manifest Constants / Defines
****************************************************************************/
/*******************************************************************************
* 2.Private constant and macro definitions using #define
*******************************************************************************/
/*register define*/
#define FTS_RESET_PIN										GPIO_CTP_RST_PIN
#define TPD_OK												0
#define DEVICE_MODE										0x00
#define GEST_ID												0x01
#define TD_STATUS											0x02
#define TOUCH1_XH											0x03
#define TOUCH1_XL											0x04
#define TOUCH1_YH											0x05
#define TOUCH1_YL											0x06
#define TOUCH2_XH											0x09
#define TOUCH2_XL											0x0A
#define TOUCH2_YH											0x0B
#define TOUCH2_YL											0x0C
#define TOUCH3_XH											0x0F
#define TOUCH3_XL											0x10
#define TOUCH3_YH											0x11
#define TOUCH3_YL											0x12
#define TPD_MAX_RESET_COUNT								3


/*
#define TPD_PROXIMITY

*/
#define FTS_CTL_IIC
#define SYSFS_DEBUG
#define FTS_APK_DEBUG

#define REPORT_TOUCH_DEBUG								0
/*
for tp esd check
*/

#if FT_ESD_PROTECT
#define TPD_ESD_CHECK_CIRCLE							200
static struct delayed_work gtp_esd_check_work;
static struct workqueue_struct *gtp_esd_check_workqueue;
static int count_irq;
static u8 run_check_91_register;
static unsigned long esd_check_circle = TPD_ESD_CHECK_CIRCLE;
static void gtp_esd_check_func(struct work_struct *);
#endif


/****************************************************************************
 * Macros
 ****************************************************************************/


/****************************************************************************
* Type Definitions
****************************************************************************/


/****************************************************************************
* Variables
****************************************************************************/
#if defined(TOUCH_MODEL_K7)
static const char defaultFirmware[] =
"focaltech/K7/FT8707/LGE_K7_FT8707_0x96_V0x08_D0x01_20151229_all.bin";
static const char defaultBootFirmware[] =
"focaltech/K7/FT8707/FT8707_Pramboot_V0.2_20151215_Final.bin";
/* #define PALM_REJECTION_ENABLE   1 */
#define CORNER_COVER_USE    1
#endif
static struct fts_ts_data *ts;
struct delayed_work work_cover;
int cover_status = 0;

#if defined(ENABLE_SWIPE_MODE)
static int get_swipe_mode = 1;
/*
static int wakeup_by_swipe = 0;
*/
#endif
static const char normal_sd_path[] = "/mnt/sdcard/touch_self_test.txt";
static const char factory_sd_path[] = "/data/logger/touch_self_test.txt";
/* extern int lge_get_factory_boot(void); */

/****************************************************************************
* Extern Function Prototypes
****************************************************************************/


/****************************************************************************
* Local Function Prototypes
****************************************************************************/


/****************************************************************************
* Local Functions
****************************************************************************/
static int firmwareUpgrade(const struct firmware *fw_img, const struct firmware *bootfw_img)
{
	u8 *pbt_buf = NULL;
	u8 *boot_pbt_buf = NULL;
	int ret;
	int fw_len = fw_img->size;
	int boot_fw_len = bootfw_img->size;

	TOUCH_LOG("firmwareUpgrade.\n");
	/*FW upgrade */
	pbt_buf = (u8 *) fw_img->data;
	boot_pbt_buf = (u8 *) bootfw_img->data;

	/*call the upgrade function */
	/* ret =  fts_ctpm_fw_upgrade(ts->client, pbt_buf, fw_len); */
	ret = fts_ctpm_auto_upgrade(ts->client, pbt_buf, fw_len, boot_pbt_buf, boot_fw_len);
	if (ret != 0) {
		TOUCH_ERR("Firwmare upgrade failed. err=%d.\n", ret);
	} else {
#ifdef AUTO_CLB
		fts_ctpm_auto_clb(ts->client);	/*start auto CLB */
#endif
	}
	return ret;
}

static void change_cover_func(struct work_struct *work_cover)
{
	u8 wbuf[4];
	struct i2c_client *client = Touch_Get_I2C_Handle();

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_WINDOW_MODE;
	wbuf[2] = cover_status;

	if (FT8707_I2C_Write(client, wbuf, 3))
		TOUCH_ERR("mip_lpwg_start failed\n");
	else
		TOUCH_LOG("FT8707_Set_CoverMode status=%d\n", cover_status);

}

void FT8707_Set_BootCoverMode(int status)
{
	INIT_DELAYED_WORK(&work_cover, change_cover_func);
	cover_status = status;
}

void FT8707_Set_CoverMode(int status)
{
	cover_status = status;
	queue_delayed_work(touch_wq, &work_cover, msecs_to_jiffies(10));
}

static void FT8707_WriteFile(char *filename, char *data, int time)
{
	int fd = 0;
	char time_string[64] = { 0 };
	struct timespec my_time;
	struct tm my_date;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	fd = sys_open(filename, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd >= 0) {
		if (time > 0) {
			my_time = __current_kernel_time();
			time_to_tm(my_time.tv_sec, sys_tz.tz_minuteswest * 60 * (-1), &my_date);
			snprintf(time_string, 64, "\n%02d-%02d %02d:%02d:%02d.%03lu\n\n\n",
				 my_date.tm_mon + 1, my_date.tm_mday, my_date.tm_hour,
				 my_date.tm_min, my_date.tm_sec,
				 (unsigned long)my_time.tv_nsec / 1000000);
			sys_write(fd, time_string, strlen(time_string));
		}
		sys_write(fd, data, strlen(data));
		sys_close(fd);
	}
	set_fs(old_fs);
}


/****************************************************************************
* Device Specific Functions
****************************************************************************/
int mip_i2c_dummy(struct i2c_client *client, char *write_buf, unsigned int write_len)
{
	int retry = 3;
	int res = 0;

	while (retry--) {
		TOUCH_FUNC();
		res = FT8707_I2C_Write(client, write_buf, write_len);
		if (res < 0) {
			TOUCH_ERR("i2c_transfer - errno[%d]\n", res);
			return TOUCH_FAIL;
		}
	}

	return TOUCH_SUCCESS;
}


int mip_lpwg_config(struct i2c_client *client)
{
	u8 wbuf[32];

	wbuf[0] = MIP_R0_LPWG;
	wbuf[1] = MIP_R1_LPWG_IDLE_REPORTRATE;
	wbuf[2] = 20;		/* LPWG_IDLE_REPORTRATE */
	wbuf[3] = 40;		/* LPWG_ACTIVE_REPORTRATE */
	wbuf[4] = 30;		/* LPWG_SENSITIVITY */
	wbuf[5] = 0 & 0xFF;	/* LPWG_ACTIVE_AREA (horizontal start low byte) */
	wbuf[6] = 0 >> 8 & 0xFF;	/* LPWG_ACTIVE_AREA (horizontal start high byte) */
	wbuf[7] = 0 & 0xFF;	/* LPWG_ACTIVE_AREA (vertical start low byte) */
	wbuf[8] = 0 >> 8 & 0xFF;	/* LPWG_ACTIVE_AREA (vertical start high byte) */
	wbuf[9] = 720 & 0xFF;	/* LPWG_ACTIVE_AREA (horizontal end low byte) */
	wbuf[10] = 720 >> 8 & 0xFF;	/* LPWG_ACTIVE_AREA (horizontal end high byte) */
	wbuf[11] = 1280 & 0xFF;	/* LPWG_ACTIVE_AREA (vertical end low byte) */
	wbuf[12] = 1280 >> 8 & 0xFF;	/* LPWG_ACTIVE_AREA (vertical end high byte) */
	wbuf[13] = LPWG_DEBUG_ENABLE;	/* LPWG_FAIL_REASON */

	if (FT8707_I2C_Write(client, wbuf, 14)) {
		TOUCH_ERR("mip_lpwg_config failed\n");
		return TOUCH_FAIL;
	}

	return 0;
}

int mip_lpwg_config_knock_on(struct i2c_client *client)
{
	u8 wbuf[32];

	wbuf[0] = MIP_R0_LPWG;
	wbuf[1] = MIP_R1_LPWG_ENABLE;
	wbuf[2] = 1;		/* LPWG_ENABLE */
	wbuf[3] = 2;		/* LPWG_TAP_COUNT */
	wbuf[4] = 10 & 0xFF;	/* LPWG_TOUCH_SLOP (low byte) */
	wbuf[5] = 10 >> 8 & 0xFF;	/* LPWG_TOUCH_SLOP (high byte) */
	wbuf[6] = 0 & 0xFF;	/* LPWG_MIN_DISTANCE (low byte) */
	wbuf[7] = 0 >> 8 & 0xFF;	/* LPWG_MIN_DISTANCE (high byte) */
	wbuf[8] = 10 & 0xFF;	/* LPWG_MAX_DISTANCE (low byte) */
	wbuf[9] = 10 >> 8 & 0xFF;	/* LPWG_MAX_DISTANCE (high byte) */
	wbuf[10] = 0 & 0xFF;	/* LPWG_MIN_INTERTAP_TIME (low byte) */
	wbuf[11] = 0 >> 8 & 0xFF;	/* LPWG_MIN_INTERTAP_TIME (high byte) */
	wbuf[12] = 700 & 0xFF;	/* LPWG_MAX_INTERTAP_TIME (low byte) */
	wbuf[13] = 700 >> 8 & 0xFF;	/* LPWG_MAX_INTERTAP_TIME (high byte) */
	wbuf[14] = (ts->lpwgSetting.isFirstTwoTapSame ? KNOCKON_DELAY : 0) & 0xFF;
	wbuf[15] = ((ts->lpwgSetting.isFirstTwoTapSame ? KNOCKON_DELAY : 0) >> 8) & 0xFF;

	if (FT8707_I2C_Write(client, wbuf, 16)) {
		TOUCH_ERR("Knock on Setting failed\n");
		return TOUCH_FAIL;
	}

	return 0;
}

int mip_lpwg_config_knock_code(struct i2c_client *client)
{
	u8 wbuf[32];

	wbuf[0] = MIP_R0_LPWG;
	wbuf[1] = MIP_R1_LPWG_ENABLE2;
	wbuf[2] = 1;		/* LPWG_ENABLE2 */
	wbuf[3] = ts->lpwgSetting.tapCount;	/* LPWG_TAP_COUNT2 */
	wbuf[4] = 10 & 0xFF;	/* LPWG_TOUCH_SLOP2 (low byte) */
	wbuf[5] = 10 >> 8 & 0xFF;	/* LPWG_TOUCH_SLOP2 (high byte) */
	wbuf[6] = 0 & 0xFF;	/* LPWG_MIN_DISTANCE2 (low byte) */
	wbuf[7] = 0 >> 8 & 0xFF;	/* LPWG_MIN_DISTANCE2 (high byte) */
	wbuf[8] = 65535 & 0xFF;	/* LPWG_MAX_DISTANCE2 (low byte) */
	wbuf[9] = 65535 >> 8 & 0xFF;	/* LPWG_MAX_DISTANCE2 (high byte) */
	wbuf[10] = 0 & 0xFF;	/* LPWG_MIN_INTERTAP_TIME2 (low byte) */
	wbuf[11] = 0 >> 8 & 0xFF;	/* LPWG_MIN_INTERTAP_TIME2 (high byte) */
	wbuf[12] = 700 & 0xFF;	/* LPWG_MAX_INTERTAP_TIME2 (low byte) */
	wbuf[13] = 700 >> 8 & 0xFF;	/* LPWG_MAX_INTERTAP_TIME2 (high byte) */
	wbuf[14] = 250 & 0xFF;	/* LPWG_INTERTAP_DELAY2 (low byte) */
	wbuf[15] = 250 >> 8 & 0xFF;	/* LPWG_INTERTAP_DELAY2 (high byte) */

	if (FT8707_I2C_Write(client, wbuf, 16)) {
		TOUCH_ERR("Knock code Setting failed\n");
		return TOUCH_FAIL;
	}

	return 0;
}

int mip_lpwg_debug_enable(struct i2c_client *client, int enable)
{
	u8 wbuf[4];

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_LPWG_DEBUG_ENABLE;
	wbuf[2] = enable;

	if (FT8707_I2C_Write(client, wbuf, 3)) {
		TOUCH_ERR("LPWG debug Setting failed\n");
		return TOUCH_FAIL;
	}

	return 0;
}

int mip_lpwg_enable_sensing(struct i2c_client *client, bool enable)
{
	u8 wbuf[4];

	wbuf[0] = MIP_R0_LPWG;
	wbuf[1] = MIP_R1_LPWG_ENABLE_SENSING;
	wbuf[2] = enable;

	if (FT8707_I2C_Write(client, wbuf, 3)) {
		TOUCH_ERR("mip_lpwg_enable_sensing failed\n");
		return TOUCH_FAIL;
	}

	return 0;
}

int mip_lpwg_start(struct i2c_client *client)
{
	u8 wbuf[4];

	wbuf[0] = MIP_R0_LPWG;
	wbuf[1] = MIP_R1_LPWG_START;
	wbuf[2] = 1;

	if (FT8707_I2C_Write(client, wbuf, 3)) {
		TOUCH_ERR("mip_lpwg_start failed\n");
		return TOUCH_FAIL;
	}

	return 0;
}

static int lpwg_control(struct i2c_client *client, TouchState newState)
{
	TOUCH_FUNC();

	switch (newState) {
	case STATE_NORMAL:
		break;

	case STATE_KNOCK_ON_ONLY:
		if (cover_status) {
			mip_lpwg_start(client);
			mip_lpwg_enable_sensing(client, 0);
			TOUCH_LOG("cover_status is closed, sensing disable\n");
		} else {
			mip_lpwg_config(client);
			mip_lpwg_config_knock_on(client);
			if (LPWG_DEBUG_ENABLE)
				mip_lpwg_debug_enable(client, 1);
			mip_lpwg_start(client);
			if (ts->currState == STATE_OFF)
				mip_lpwg_enable_sensing(client, 1);
		}
		break;

	case STATE_KNOCK_ON_CODE:
		if (cover_status) {
			mip_lpwg_start(client);
			mip_lpwg_enable_sensing(client, 0);
			TOUCH_LOG("cover_status is closed, sensing disable\n");
		} else {
			mip_lpwg_config(client);
			mip_lpwg_config_knock_on(client);
			mip_lpwg_config_knock_code(client);
			if (LPWG_DEBUG_ENABLE)
				mip_lpwg_debug_enable(client, 1);
			mip_lpwg_start(client);
			if (ts->currState == STATE_OFF)
				mip_lpwg_enable_sensing(client, 1);
		}
		break;

	case STATE_OFF:
		mip_lpwg_start(client);
		mip_lpwg_enable_sensing(client, 0);
		break;

	default:
		TOUCH_ERR("invalid touch state ( %d )\n", newState);
		break;
	}

	return TOUCH_SUCCESS;
}

static ssize_t show_rawdata(TouchDriverData *pDriverData, char *buf)
{
	int ret = 0;

	if (pDriverData == NULL)
		TOUCH_ERR("failed to get pDriverData for rawdata test\n");

	TOUCH_FUNC();
	/* temp block */
	return ret;

	/* rawdata check */
	/* ret = FT8707_GetTestResult(client, buf, &rawdataStatus, RAW_DATA_SHOW); */
	if (ret < 0) {
		TOUCH_ERR("failed to get raw data\n");
		ret = sprintf(buf, "failed to get raw data\n");
	}

	return ret;
}

static ssize_t show_intensity(TouchDriverData *pDriverData, char *buf)
{
	int ret = 0;

	if (pDriverData == NULL)
		TOUCH_ERR("failed to get pDriverData for intensity test\n");

	TOUCH_FUNC();
	/* temp block */
	return ret;
	/* intensity check */
	/* ret = FT8707_GetTestResult(client, buf, &intensityStatus, INTENSITY_SHOW); */
	if (ret < 0) {
		TOUCH_ERR("failed to get intensity data\n");
		ret = sprintf(buf, "failed to get intensity data\n");
	}

	return ret;
}

static ssize_t store_reg_control(TouchDriverData *pDriverData, const char *buf, size_t count)
{
	struct i2c_client *client = Touch_Get_I2C_Handle();
	u8 reg_addr = { 0 };
	int offset = 0;
	char cmd[10] = { 0, };
	int value = 0;
	u8 r_buf[50] = { 0 };
	u8 w_buf[51] = { 0, };

	if (sscanf(buf, "%s %s ", cmd, &reg_addr) != 4) {
		TOUCH_LOG("data parsing fail.\n");
		TOUCH_LOG("Usage :\n");
		TOUCH_LOG("read reg offset value\n");
		TOUCH_LOG("write reg offset value\n");
		return count;
	}

	TOUCH_LOG("reg ctrl : %s, 0x%02x, %d, %d\n", cmd, reg_addr, offset, value);

	/* temp block */
	return count;

	mutex_lock(&pDriverData->thread_lock);
	if (!strcmp(cmd, "read")) {
		FT8707_I2C_Read(client, &reg_addr, 1, r_buf, (offset + 1));
		TOUCH_LOG("Read : reg_addr[0x%02x] offset[%d] value[0x%02x]\n", reg_addr, offset,
			  r_buf[offset]);
	} else if (!strcmp(cmd, "write")) {
		FT8707_I2C_Read(client, &reg_addr, 1, r_buf, (offset + 1));
		r_buf[offset] = value;
		w_buf[0] = reg_addr;
		memcpy(&w_buf[1], r_buf, offset + 1);
		FT8707_I2C_Write(client, w_buf, offset + 2);
		TOUCH_LOG("Write : reg_addr[0x%02x] offset[%d] value[0x%02x]\n", reg_addr, offset,
			  w_buf[offset]);
	} else {
		TOUCH_LOG("Usage :\n");
		TOUCH_LOG("read reg value offset\n");
		TOUCH_LOG("write reg value offset\n");
	}
	mutex_unlock(&pDriverData->thread_lock);
#if 0
	switch (cmd) {
	case 1:
		write_buf[0] = reg_addr[0];
		write_buf[1] = reg_addr[1];
		if (FT8707_I2C_Read(client, write_buf, len, read_buf, value))
			TOUCH_LOG("store_reg_control failed\n");

		for (i = 0; i < value; i++)
			TOUCH_LOG("read_buf=[%d]\n", read_buf[i]);
		break;

	case 2:
		write_buf[0] = reg_addr[0];
		write_buf[1] = reg_addr[1];
		if (value >= 256) {
			write_buf[2] = (value >> 8);
			write_buf[3] = (value & 0xFF);
			len = len + 2;
		} else {
			write_buf[2] = value;
			len++;
		}
		if (FT8707_I2C_Write(client, write_buf, len))
			TOUCH_ERR("store_reg_control failed\n");

		break;
	default:
		break;
	}
#endif
	return count;
}

static ssize_t store_ftk_fw_upgrade(TouchDriverData *pDriverData, const char *buf, size_t count)
{
	char boot_name[MAX_FILENAME] =
	    "focaltech/K7/FT8707/FT8707_Pramboot_V0.2_20151215_Final.bin";
	char app_name[MAX_FILENAME] =
	    "focaltech/K7/FT8707/LGE_K7_FT8707_0x96_V0x08_D0x01_20151229_all.bin";
	const struct firmware *boot_fw = NULL;
	const struct firmware *app_fw = NULL;
	int ret = 0;

	if (count > MAX_FILENAME)
		return count;
#if 0
	if (sscanf(buf, "%s %s", boot_name, app_name) != 2) {
		TOUCH_LOG("f/w name parsing fail.\n");
		return count;
	}
#endif
	TOUCH_LOG("ftk_fw_upgrade : boot = %s, app = %s\n", boot_name, app_name);

	mutex_lock(&pDriverData->thread_lock);
	if (boot_name != NULL && app_name != NULL) {
		/* Update Firmware */
		ret = FT8707_RequestFirmware(app_name, boot_name, &app_fw, &boot_fw);
		if (ret < 0) {
			TOUCH_LOG("Fail to request F/W.\n");
			goto EXIT;
		}
		ret = firmwareUpgrade(app_fw, boot_fw);
		if (ret < 0) {
			TOUCH_LOG("Firmware Upgrade Fail.\n");
			release_firmware(boot_fw);
			release_firmware(app_fw);
			goto EXIT;
		}

		release_firmware(boot_fw);
		release_firmware(app_fw);

	} else {
		TOUCH_LOG("Error : can't find fw, boot = %s, app = %s\n", boot_name, app_name);
		goto EXIT;
	}
	mutex_unlock(&pDriverData->thread_lock);
	return count;

EXIT:
	mutex_unlock(&pDriverData->thread_lock);
	return count;
}

static ssize_t store_ftk_reset(TouchDriverData *pDriverData, const char *buf, size_t count)
{
	if (count > MAX_FILENAME)
		return count;

	TOUCH_LOG("ftk_reset .\n");

	mutex_lock(&pDriverData->thread_lock);
	FT8707_Reset(0, 80);
	FT8707_Reset(1, 150);
	mutex_unlock(&pDriverData->thread_lock);
	return count;
}


static LGE_TOUCH_ATTR(intensity, S_IRUGO | S_IWUSR, show_intensity, NULL);
static LGE_TOUCH_ATTR(rawdata, S_IRUGO | S_IWUSR, show_rawdata, NULL);
static LGE_TOUCH_ATTR(reg_control, S_IRUGO | S_IWUSR, NULL, store_reg_control);
static LGE_TOUCH_ATTR(ftk_fw_upgrade, S_IRUGO | S_IWUSR, NULL, store_ftk_fw_upgrade);
static LGE_TOUCH_ATTR(ftk_reset, S_IRUGO | S_IWUSR, NULL, store_ftk_reset);



static struct attribute *FT8707_attribute_list[] = {
	&lge_touch_attr_intensity.attr,
	&lge_touch_attr_rawdata.attr,
	&lge_touch_attr_reg_control.attr,
	&lge_touch_attr_ftk_fw_upgrade.attr,
	&lge_touch_attr_ftk_reset.attr,
	NULL,
};


static int FT8707_Initialize(TouchDriverData *pDriverData)
{
	struct i2c_client *client = Touch_Get_I2C_Handle();
	int err = 0;
	u8 data = 0;
	u8 read_addr = 0;
	int reset_count = 0;

	TOUCH_FUNC();

	/* IMPLEMENT : Device initialization at Booting */
	ts = devm_kzalloc(&client->dev, sizeof(struct fts_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		TOUCH_ERR("failed to allocate memory for device driver data\n");
		return TOUCH_FAIL;
	}

	ts->client = client;

	/* Need to Implement add misc driver register */

	/* F/W Update Check */
/* RE_TRY: */
	read_addr = 0x00;
	/* if auto upgrade fail, it will not read right value next upgrade. */
	err = FT8707_I2C_Read(client, &read_addr, 1, &data, 1);
	/* err = fts_read_reg(client, 0x00, &data); */

	TOUCH_LOG("[%s] %d,data:%d\n", __func__, err, data);

	if (err < 0 || data != 0) {	/* reg0 data running state is 0; other state is not 0 */
		TOUCH_LOG("I2C transfer error, line: %d\n", __LINE__);

		if (++reset_count < TPD_MAX_RESET_COUNT) {
			TOUCH_LOG("Reset Process\n");
/* FT8707_Reset(0, 80); */
/* FT8707_Reset(1, 130); */
/* goto RE_TRY; */
		}
	}

	fts_get_upgrade_array(client);	/* flash */
#ifdef FTS_CTL_IIC
	if (fts_rw_iic_drv_init(client) < 0)
		TOUCH_LOG("%s:[FTS] create fts control iic driver failed\n", __func__);
#endif

#ifdef FTS_APK_DEBUG
	fts_create_apk_debug_channel(client);
#endif


#if 0				/* ESD Protection */
	INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);
	gtp_esd_check_workqueue = create_workqueue("gtp_esd_check");
	queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
#endif

/* fts_create_sysfs(client, pDriverData); */
	return TOUCH_SUCCESS;
}

void FT8707_Reset(int status, int delay)
{
	if (!status)
		TouchDisableIrq();
	TouchSetGpioReset(status);
	if (delay <= 0 || delay > 1000) {
		TOUCH_LOG("%s exeeds limit %d\n", __func__, delay);
		if (status)
			TouchEnableIrq();
		return;
	}

	if (delay <= 20)
		mdelay(delay);
	else
		msleep(delay);
	if (status) {
		TouchEnableIrq();
		if (cover_status)
			queue_delayed_work(touch_wq, &work_cover, msecs_to_jiffies(10));
	}
}

static void FT8707_Reset_Dummy(void)
{
	FT8707_Reset(0, 80);
	FT8707_Reset(1, 150);
}

static int FT8707_InitRegister(void)
{

	/* IMPLEMENT : Register initialization after reset */

	return TOUCH_SUCCESS;
}

static int FT8707_InterruptHandler(TouchReadData *pData)
{
	TouchFingerData *pFingerData = NULL;
	u8 i = 0;
	u8 num_finger = 0;
	u8 buf[POINT_READ_BUF] = { 0, };
	u8 pointid = FTS_MAX_ID;

	u8 packet_type = 0;
	u8 index = 0;
	u8 state = 0;
	struct i2c_client *client = Touch_Get_I2C_Handle();

	pData->type = DATA_UNKNOWN;
	pData->count = 0;

#if 0
	if (LPWG_DEBUG_ENABLE == 0
	    && (ts->currState == STATE_KNOCK_ON_ONLY || ts->currState == STATE_KNOCK_ON_CODE)) {
		TOUCH_LOG("Fail To Send Dummy Packet\n");

		if (mip_i2c_dummy(client, wbuf, 2) == TOUCH_FAIL) {
			TOUCH_ERR("Fail to send dummy packet\n");
			goto ERROR;
		}
	}
#endif
	/* fts_read_reg(client, 0x01, &packet_type); */
	/* TOUCH_LOG("Interrupt Type : %d\n", packet_type); */
	/* TOUCH_LOG("Interrupt Type : %d\n", packet_type); */
	FT8707_I2C_Read(client, buf, 1, buf, POINT_READ_BUF);

	num_finger = buf[FT_TOUCH_POINT_NUM];
	/* TOUCH_LOG("num_finger = %d.\n", num_finger); */
	if (num_finger > 10) {
		TOUCH_LOG("num_finger over max : %d\n", num_finger);
		goto ERROR;
	}
	/* Event handler */
	if (packet_type == 0x00) {	/* Touch event */
#if 0
		FT8707_I2C_Read(client, buf, 1, buf, POINT_READ_BUF);

		num_finger = buf[FT_TOUCH_POINT_NUM];
		if (num_finger > 10) {
			TOUCH_LOG("num_finger over max : %d\n", num_finger);
			goto ERROR;
		}
#endif
		for (i = 0; i < fts_updateinfo_curr.TPD_MAX_POINTS; i++) {
			pointid = (buf[FTS_TOUCH_ID_POS + (FTS_TOUCH_STEP * i)]) >> 4;
#if 0				/*for debug */
			TOUCH_LOG(" i = %d , point id = %d.\n", i, pointid);
			TOUCH_LOG(" i = %d , max point  = %d.\n", i,
				  fts_updateinfo_curr.TPD_MAX_POINTS);
#endif
			if (pointid >= FTS_MAX_ID) {
				TOUCH_LOG("FTS_MAX_ID pointid = %d\n", pointid);
				break;
			}

			state = (buf[FTS_TOUCH_EVENT_POS + FTS_TOUCH_STEP * i] & 0xC0) >> 6;

			if ((index < 0) || (index > MAX_NUM_OF_FINGERS - 1)) {
				TOUCH_ERR("invalid touch index (%d)\n", index);
				goto ERROR;
			}

			pData->type = DATA_FINGER;
#if 0
			if ((tmp[0] & MIP_EVENT_INPUT_PALM) >> 4) {
				if (state)
					TOUCH_LOG("Palm detected : %d\n", tmp[4]);
				else
					TOUCH_LOG("Palm released : %d\n", tmp[4]);

				return TOUCH_SUCCESS;
			}
#endif
			if (state == FTK_TOUCH_DOWN || state == FTK_TOUCH_CONTACT) {
				pFingerData = &pData->fingerData[pointid];
				pFingerData->id = pointid;
				pFingerData->x =
				    (u16) (buf[FTS_TOUCH_X_H_POS + FTS_TOUCH_STEP * i] & 0x0F) << 8
				    | (u16) buf[FTS_TOUCH_X_L_POS + FTS_TOUCH_STEP * i];
				pFingerData->y =
				    (u16) (buf[FTS_TOUCH_Y_H_POS + FTS_TOUCH_STEP * i] & 0x0F) << 8
				    | (u16) buf[FTS_TOUCH_Y_L_POS + FTS_TOUCH_STEP * i];
				pFingerData->width_major =
				    (buf[FTS_TOUCH_MISC + FTS_TOUCH_STEP * i]) >> 4;
				pFingerData->width_minor = 0;
				pFingerData->orientation = 0;
				pFingerData->pressure = (buf[FTS_TOUCH_XY_POS + FTS_TOUCH_STEP * i]);
#if 0				/* for debug */
				TOUCH_LOG
				    ("id = %d, x = %d, y = %d, width_major = %d, pressure = %d\n",
				     pFingerData->id, pFingerData->x, pFingerData->y,
				     pFingerData->width_major, pFingerData->pressure);
#endif
				if (pFingerData->pressure < 1)
					pFingerData->pressure = 1;
				else if (pFingerData->pressure > 255 - 1)
					pFingerData->pressure = 255 - 1;

				pData->count++;
				pFingerData->status = FINGER_PRESSED;
			} else if (state == FTK_TOUCH_UP || state == FTK_TOUCH_NO_EVENT) {
				pFingerData = &pData->fingerData[pointid];
				pFingerData->id = pointid;
				pFingerData->status = FINGER_RELEASED;
#if 0				/*for debug */
				TOUCH_LOG("point_id = %d\n", pointid);
#endif
			}

		}
	} else {		/* Alert event */

		/*FTK Need To Implement Knock On / Code */
#if 0
		alert_type = rbuf[0];

		if (alert_type == MIP_ALERT_ESD) {
			TOUCH_LOG("ESD Detected!\n");
		} else if (alert_type == MIP_ALERT_WAKEUP) {
			if (rbuf[1] == MIP_EVENT_GESTURE_DOUBLE_TAP) {
				TOUCH_LOG("Knock-on Detected\n");
				pData->type = DATA_KNOCK_ON;
			} else if (rbuf[1] == MIP_EVENT_GESTURE_MULTI_TAP) {
				TOUCH_LOG("Knock-code Detected\n");
				pData->type = DATA_KNOCK_CODE;

				for (i = 2; i < packet_size; i += 3) {
					u8 *tmp = &rbuf[i];

					pData->knockData[((i + 1) / 3) - 1].x =
					    tmp[1] | ((tmp[0] & 0xf) << 8);
					pData->knockData[((i + 1) / 3) - 1].y =
					    tmp[2] | (((tmp[0] >> 4) & 0xf) << 8);
					pData->count++;
				}
			} else {
				/* Re-enter tap mode */
				wbuf[0] = MIP_R0_CTRL;
				wbuf[1] = MIP_R1_CTRL_POWER_STATE;
				wbuf[2] = MIP_CTRL_POWER_LOW;
				if (FT8707_I2C_Write(client, wbuf, 3)) {
					TOUCH_ERR("mip_i2c_write failed\n");
					goto ERROR;
				}
			}

		} else if (alert_type == MIP_ALERT_F1) {
			/*FTK Need to Implement Knock On / Code Fail Reason */

			if (rbuf[1] == MIP_LPWG_EVENT_TYPE_FAIL) {
				switch (rbuf[2]) {
				case OUT_OF_AREA:
					TOUCH_LOG("LPWG FAIL REASON = Out of Area\n");
					break;
				case PALM_DETECTED:
					TOUCH_LOG("LPWG FAIL REASON = Palm\n");
					break;
				case DELAY_TIME:
					TOUCH_LOG("LPWG FAIL REASON = Delay Time\n");
					break;
				case TAP_TIME:
					TOUCH_LOG("LPWG FAIL REASON = Tap Time\n");
					break;
				case TAP_DISTACE:
					TOUCH_LOG("LPWG FAIL REASON = Tap Distance\n");
					break;
				case TOUCH_SLOPE:
					TOUCH_LOG("LPWG FAIL REASON = Touch Slope\n");
					break;
				case MULTI_TOUCH:
					TOUCH_LOG("LPWG FAIL REASON = Multi Touch\n");
					break;
				case LONG_PRESS:
					TOUCH_LOG("LPWG FAIL REASON = Long Press\n");
					break;
				default:
					TOUCH_LOG("LPWG FAIL REASON = Unknown Reason\n");
					break;
				}
			} else {
				/* Re-enter tap mode */
				wbuf[0] = MIP_R0_CTRL;
				wbuf[1] = MIP_R1_CTRL_POWER_STATE;
				wbuf[2] = MIP_CTRL_POWER_LOW;
				if (FT8707_I2C_Write(client, wbuf, 3)) {
					TOUCH_ERR("mip_i2c_write failed\n");
					goto ERROR;
				}
			}

		} else {
			/* TOUCH_LOG("Unknown alert type [%d]\n", alert_type); */
			goto ERROR;
		}
#endif
	}

	return TOUCH_SUCCESS;

ERROR:
	FT8707_Reset(0, 10);
	FT8707_Reset(1, 150);
	return TOUCH_FAIL;
}

static int FT8707_ReadIcFirmwareInfo(TouchFirmwareInfo *pFwInfo)
{
	u8 version[2] = { 0, };
	u8 version_buf = 0;
	int ret = 0;
	struct i2c_client *client = Touch_Get_I2C_Handle();

	TOUCH_FUNC();

	/* IMPLEMENT : read IC firmware information function */
#if 0
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_VERSION_CUSTOM;

	ret = FT8707_I2C_Read(client, wbuf, 2, version, 2);
#endif
	ret = fts_read_reg(client, FTS_REG_FW_VER, &version_buf);
	version[0] = (version_buf & 0x80) >> 7;
	version[1] = version_buf & 0x7F;

	if (ret == TOUCH_FAIL)
		return TOUCH_FAIL;

	pFwInfo->moduleMakerID = 0;
	pFwInfo->moduleVersion = 0;
	pFwInfo->modelID = 0;
	pFwInfo->isOfficial = version[0];
	pFwInfo->version = version[1];

	TOUCH_LOG("IC F/W Version = v%X.%02X ( %s )\n", version[0], version[1],
		  pFwInfo->isOfficial ? "Official Release" : "Test Release");

	return TOUCH_SUCCESS;
}

static int FT8707_GetBinFirmwareInfo(char *pFilename, TouchFirmwareInfo *pFwInfo)
{
	int ret = 0;
	const struct firmware *fw = NULL;
	u8 version[2] = { 0, };
	u8 *pFwFilename = NULL;
	struct i2c_client *client = Touch_Get_I2C_Handle();

	TOUCH_FUNC();
	/* block touch f/W upgrade */
	/* return TOUCH_SUCCESS; */

	if (pFilename == NULL)
		pFwFilename = (char *)defaultFirmware;
	else
		pFwFilename = pFilename;

	TOUCH_LOG("Firmware filename = %s\n", pFwFilename);

	/* Get firmware image buffer pointer from file */
	ret = request_firmware(&fw, pFwFilename, &client->dev);
	if (ret) {
		TOUCH_ERR("failed at request_firmware() ( error = %d )\n", ret);
		return TOUCH_FAIL;
	}
/* mip_bin_fw_version(ts, fw->data, fw->size, version); */

	/* Need to Parsing Product Code */

	/* Firmware Bin Version */
	version[0] = (fw->data[0x2000 + 0x10e] & 0x80) >> 7;
	version[1] = fw->data[0x2000 + 0x10e] & 0x7F;


	pFwInfo->moduleMakerID = 0;
	pFwInfo->moduleVersion = 0;
	pFwInfo->modelID = 0;
	pFwInfo->isOfficial = version[0];
	pFwInfo->version = version[1];

	/* Free firmware image buffer */
	release_firmware(fw);

	TOUCH_LOG("BIN F/W Version = v%X.%02X ( %s )\n", version[0], version[1],
		  pFwInfo->isOfficial ? "Official Release" : "Test Release");

	return TOUCH_SUCCESS;
}

void FT8707_Get_DefaultFWName(char **app_name, char **boot_name)
{
	*app_name = (char *)defaultFirmware;
	*boot_name = (char *)defaultBootFirmware;

	TOUCH_LOG("Default F/W Name Get Done.\n");
}

int FT8707_RequestFirmware(char *name_app_fw, char *name_boot_fw, const struct firmware **fw_app,
			   const struct firmware **fw_boot)
{
	int ret = 0;
	struct i2c_client *client = Touch_Get_I2C_Handle();

	/* Get firmware image buffer pointer from file */
	if (name_app_fw != NULL)
		ret = request_firmware(fw_app, name_app_fw, &client->dev);

	if (ret < 0) {
		TOUCH_LOG("Request App F/W Fail\n");
		goto FW_RQ_FAIL;
	}

	/* Get Boot firmware image buffer pointer from file */
	if (name_boot_fw != NULL)
		ret = request_firmware(fw_boot, name_boot_fw, &client->dev);

	if (ret < 0) {
		TOUCH_LOG("Request Boot F/W Fail\n");
		goto FW_APP_FAIL;
	}

	return TOUCH_SUCCESS;

FW_APP_FAIL:
	release_firmware(*fw_app);
FW_RQ_FAIL:
	return TOUCH_FAIL;

}

static int FT8707_UpdateFirmware(char *pFilename)
{
	int ret = 0;
	char *pFwFilename = NULL;
	char *pBootFwFilename = NULL;
	const struct firmware *fw = NULL;
	const struct firmware *bootFw = NULL;
#if 1
	int i = 0;
#endif
	TOUCH_FUNC();
	/* block touch f/W upgrade */
/* return TOUCH_SUCCESS; */

	if (pFilename == NULL)
		pFwFilename = (char *)defaultFirmware;
	else
		pFwFilename = pFilename;

	mdelay(500);

	if (pBootFwFilename == NULL) {
		pBootFwFilename = (char *)defaultBootFirmware;
	} else {
		/* pBootFwFilename = pFilename; */
		/* need to implement */
	}

	TOUCH_LOG("F/W Firmware = %s\n", pFwFilename);
	TOUCH_LOG("Boot F/W Firmware = %s\n", pBootFwFilename);

	/* request app, boot F/W */
	ret = FT8707_RequestFirmware(pFwFilename, pBootFwFilename, &fw, &bootFw);
	if (ret == TOUCH_FAIL) {
		TOUCH_ERR("Fail to Request App, Boot F/W Fail\n");
		return TOUCH_FAIL;
	}

	/* Do firmware upgrade */
#if 1
	/* for debug */
	for (i = 0; i < 20; i++)
		TOUCH_LOG("fw[%d] = 0x%02x\n", i, fw->data[i]);

	TOUCH_LOG("========== .\n");
	for (i = 0; i < 20; i++)
		TOUCH_LOG("boot_fw[%d] = 0x%02x\n", i, bootFw->data[i]);

	ret = firmwareUpgrade(fw, bootFw);
	if (ret < 0) {
		TOUCH_LOG("F/W Upgrade Fail.\n ");
		goto FW_UPDATE_FAIL;
	}
#endif
	/* Free firmware image buffer */
	release_firmware(bootFw);
	release_firmware(fw);

	return TOUCH_SUCCESS;

FW_UPDATE_FAIL:
	release_firmware(bootFw);
	release_firmware(fw);
	return TOUCH_FAIL;

}

static int FT8707_SetLpwgMode(TouchState newState, LpwgSetting *pLpwgSetting)
{
	int ret = TOUCH_SUCCESS;
	struct i2c_client *client = Touch_Get_I2C_Handle();
	u8 deep_sleep[2] = { 0xA5, 0x03 };

	TOUCH_FUNC();

	memcpy(&ts->lpwgSetting, pLpwgSetting, sizeof(LpwgSetting));

	if (ts->currState == newState) {
		TOUCH_LOG("device state is same as driver requested\n");
		return TOUCH_SUCCESS;
	}

	/* Block SetLPWGMode */
	/* return TOUCH_SUCCESS; */


	if ((newState < STATE_NORMAL) && (newState > STATE_KNOCK_ON_CODE)) {
		TOUCH_LOG("invalid request state ( state = %d )\n", newState);
		return TOUCH_FAIL;
	}
	if (0) {
		ret = lpwg_control(client, newState);
		if (ret == TOUCH_FAIL) {
			TOUCH_ERR("failed to set lpwg mode in device\n");
			return TOUCH_FAIL;
		}
	} else {
		if (ret == TOUCH_SUCCESS)
			ts->currState = newState;

		switch (newState) {
		case STATE_NORMAL:
			TOUCH_LOG("device was set to NORMAL\n");
			break;
		case STATE_OFF:
			FT8707_I2C_Write(client, deep_sleep, 2);
			TOUCH_LOG("device was set to OFF\n");
			break;
		case STATE_KNOCK_ON_ONLY:
			FT8707_I2C_Write(client, deep_sleep, 2);
			TOUCH_LOG("device was set to KNOCK_ON_ONLY\n");
			break;
		case STATE_KNOCK_ON_CODE:
			FT8707_I2C_Write(client, deep_sleep, 2);
			TOUCH_LOG("device was set to KNOCK_ON_CODE\n");
			break;
		default:
			TOUCH_LOG("impossilbe state ( state = %d )\n", newState);
			ret = TOUCH_FAIL;
			break;
		}
	}
	return TOUCH_SUCCESS;
}

static int FT8707_DoSelfDiagnosis(int *pRawStatus, int *pChannelStatus, char *pBuf, int bufSize,
				  int *pDataLen)
{
	/* CAUTION : be careful not to exceed buffer size */
	char *sd_path = "/mnt/sdcard/touch_self_test.txt";
	int ret = 0;
	int dataLen = 0;

	memset(pBuf, 0, bufSize);
	*pDataLen = 0;
	TOUCH_FUNC();
	/* CAUTION : be careful not to exceed buffer size */

	/* IMPLEMENT : self-diagnosis function */
/*
    if (lge_get_factory_boot() == 1) {
	sd_path = (char *) factory_sd_path;
    } else {
	sd_path = (char *) normal_sd_path;
    }
*/
	*pRawStatus = TOUCH_SUCCESS;
	*pChannelStatus = TOUCH_SUCCESS;

	FT8707_WriteFile(sd_path, pBuf, 1);
	msleep(30);

	/* raw data check */
/* ret = FT8707_GetTestResult(client, pBuf, pRawStatus, RAW_DATA_SHOW); */
	if (ret < 0) {
		TOUCH_ERR("failed to get raw data\n");
		memset(pBuf, 0, bufSize);
		*pRawStatus = TOUCH_FAIL;
	}
	FT8707_WriteFile(sd_path, pBuf, 0);
	msleep(30);
	memset(pBuf, 0, bufSize);

	/* cm_delta check */
/* ret = FT8707_GetTestResult(client, pBuf, pChannelStatus, DELTA_SHOW); */
	if (ret < 0) {
		TOUCH_ERR("failed to get delta data\n");
		memset(pBuf, 0, bufSize);
		*pChannelStatus = TOUCH_FAIL;
	}
	FT8707_WriteFile(sd_path, pBuf, 0);
	msleep(30);
	memset(pBuf, 0, bufSize);

	/* cm_jitter check */
/* ret = FT8707_GetTestResult(client, pBuf, pChannelStatus, JITTER_SHOW); */
	if (ret < 0) {
		TOUCH_ERR("failed to get jitter data\n");
		memset(pBuf, 0, bufSize);
		*pChannelStatus = TOUCH_FAIL;
	}
	FT8707_WriteFile(sd_path, pBuf, 0);
	msleep(30);
	memset(pBuf, 0, bufSize);

	/* open short check */
/* ret = FT8707_GetTestResult(client, pBuf, pChannelStatus, OPENSHORT_SHOW); */
	if (ret < 0) {
		TOUCH_ERR("failed to get open short data\n");
		memset(pBuf, 0, bufSize);
		*pChannelStatus = TOUCH_FAIL;
	}
	FT8707_WriteFile(sd_path, pBuf, 0);
	msleep(30);
	memset(pBuf, 0, bufSize);

	/* open short 2 check (MUX) */
/* ret = FT8707_GetTestResult(client, pBuf, pChannelStatus, MUXSHORT_SHOW); */
	if (ret < 0) {
		TOUCH_ERR("failed to get open short (mux) data\n");
		memset(pBuf, 0, bufSize);
		*pChannelStatus = TOUCH_FAIL;
	}
	FT8707_WriteFile(sd_path, pBuf, 0);
	msleep(30);
	memset(pBuf, 0, bufSize);

	*pDataLen = dataLen;

	return TOUCH_SUCCESS;
}

#if 0
static int FT8707_DoSelfDiagnosis_Lpwg(int *lpwgStatus, char *pBuf, int bufSize, int *pDataLen)
{
	char *sd_path = "/mnt/sdcard/touch_self_test.txt";
	int ret = 0;
	int dataLen = 0;
	struct i2c_client *client = Touch_Get_I2C_Handle();
	int deltaStatus = 0;
	int jitterStatus = 0;

	memset(pBuf, 0, bufSize);
	*pDataLen = 0;
	TOUCH_FUNC();
/*
    if (lge_get_factory_boot() == 1) {
	sd_path = (char *) factory_sd_path;
    } else {
	sd_path = (char *) normal_sd_path;
    }
*/
	mip_lpwg_enable_sensing(client, 1);
	msleep(1000);

	mip_lpwg_debug_enable(client, 1);
	usleep_range(1000, 1100);

	mip_lpwg_start(client);
	usleep_range(1000, 1100);

	*lpwgStatus = TOUCH_SUCCESS;

	FT8707_WriteFile(sd_path, pBuf, 1);
	msleep(30);

	/* cm_delta check */
/* ret = FT8707_GetTestResult(client, pBuf, &deltaStatus, LPWG_JITTER_SHOW); */
	if (ret < 0) {
		TOUCH_ERR("failed to get delta data\n");
		memset(pBuf, 0, bufSize);
		deltaStatus = TOUCH_FAIL;
	}
	FT8707_WriteFile(sd_path, pBuf, 0);
	msleep(30);
	memset(pBuf, 0, bufSize);

	/* cm_jitter check */
/* ret = FT8707_GetTestResult(client, pBuf, &jitterStatus, LPWG_ABS_SHOW); */
	if (ret < 0) {
		TOUCH_ERR("failed to get jitter data\n");
		memset(pBuf, 0, bufSize);
		jitterStatus = TOUCH_FAIL;
	}
	FT8707_WriteFile(sd_path, pBuf, 0);
	msleep(30);
	memset(pBuf, 0, bufSize);

	dataLen +=
	    sprintf(pBuf, "LPWG Test: %s",
		    ((deltaStatus + jitterStatus) == TOUCH_SUCCESS) ? "Pass\n" : "Fail\n");
	*lpwgStatus = (deltaStatus + jitterStatus);

	*pDataLen = dataLen;

	mip_lpwg_debug_enable(client, 0);
	usleep_range(1000, 1100);
	mip_lpwg_start(client);
	return TOUCH_SUCCESS;
	/*
	   error :
	   mip_lpwg_debug_enable(client, 0);
	   usleep_range(1000, 1100);
	   mip_lpwg_start(client);
	   return TOUCH_FAIL; */
}
#endif
static void FT8707_PowerOn(int isOn)
{
}

static void FT8707_ClearInterrupt(void)
{

}

static void FT8707_NotifyHandler(TouchNotify notify, int data)
{

}

TouchDeviceControlFunction FT8707_Func = {
#if 0
	.Power = FT8707_PowerOn,
	.Initialize = FT8707_Initialize,
	.Reset = FT8707_Reset_Dummy,
	.InitRegister = FT8707_InitRegister,
	.ClearInterrupt = FT8707_ClearInterrupt,
	.InterruptHandler = FT8707_InterruptHandler,
	.ReadIcFirmwareInfo = FT8707_ReadIcFirmwareInfo,
	.GetBinFirmwareInfo = FT8707_GetBinFirmwareInfo,
	.UpdateFirmware = FT8707_UpdateFirmware,
	.SetLpwgMode = FT8707_SetLpwgMode,
	.DoSelfDiagnosis = FT8707_DoSelfDiagnosis,
	.DoSelfDiagnosis_Lpwg = FT8707_DoSelfDiagnosis_Lpwg,
	.device_attribute_list = FT8707_attribute_list,
	.NotifyHandler = FT8707_NotifyHandler,
#else
	.Power = FT8707_PowerOn,	/* empty */
	.Initialize = FT8707_Initialize,	/* need to implement ftk probe */
	.Reset = FT8707_Reset_Dummy,	/* empty */
	.InitRegister = FT8707_InitRegister,	/* empty */
	.ClearInterrupt = FT8707_ClearInterrupt,	/* empty */
	.InterruptHandler = FT8707_InterruptHandler,	/* Handle Interrupt */
	.ReadIcFirmwareInfo = FT8707_ReadIcFirmwareInfo,	/* Read F/W IC Info */
	.GetBinFirmwareInfo = FT8707_GetBinFirmwareInfo,	/* Read F/W Bin Info */
	.UpdateFirmware = FT8707_UpdateFirmware,	/* Firmware Update */
	.SetLpwgMode = FT8707_SetLpwgMode,	/* set LPWG Mode */
	.DoSelfDiagnosis = FT8707_DoSelfDiagnosis,	/* Do Self Diagnosis */
	.device_attribute_list = FT8707_attribute_list,	/* attribute list add */
	.NotifyHandler = FT8707_NotifyHandler,	/* empty */
#endif

};


/* End Of File */
