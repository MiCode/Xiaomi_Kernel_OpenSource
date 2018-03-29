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
#define LGTP_MODULE "[MIT300]"

/****************************************************************************
* Include Files
****************************************************************************/
#include <lgtp_common.h>

#include <lgtp_common_driver.h>
#include <lgtp_platform_api_i2c.h>
#include <lgtp_platform_api_misc.h>
#include <lgtp_device_mit300.h>


/****************************************************************************
* Manifest Constants / Defines
****************************************************************************/


/****************************************************************************
 * Macros
 ****************************************************************************/


/****************************************************************************
* Type Definitions
****************************************************************************/


/****************************************************************************
* Variables
****************************************************************************/
#if defined(TOUCH_MODEL_M1V)
static const char defaultFirmware[] = "melfas/mit300/mm1v/L0M50P1_00_01.bin";
#elif defined(TOUCH_MODEL_M4)
static const char defaultFirmware[] = "melfas/mit300/m4/L0M50P1_00_01.bin";
#endif

static struct melfas_ts_data *ts;


/****************************************************************************
* Extern Function Prototypes
****************************************************************************/


/****************************************************************************
* Local Function Prototypes
****************************************************************************/


/****************************************************************************
* Local Functions
****************************************************************************/
static void MIT300_WriteFile(char *filename, char *data, int time)
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
		res = Mit300_I2C_Write(client, write_buf, write_len);
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

	if (Mit300_I2C_Write(client, wbuf, 14)) {
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
	wbuf[14] = (DELAY_ENABLE ? KNOCKON_DELAY : 0) & 0xFF;	/* LPWG_INTERTAP_DELAY (low byte) */
	wbuf[15] = ((DELAY_ENABLE ? KNOCKON_DELAY : 0) >> 8) & 0xFF;	/* LPWG_INTERTAP_DELAY (high byte) */

	if (Mit300_I2C_Write(client, wbuf, 16)) {
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

	if (Mit300_I2C_Write(client, wbuf, 16)) {
		TOUCH_ERR("Knock code Setting failed\n");
		return TOUCH_FAIL;
	}

	return 0;
}

int mip_lpwg_debug_enable(struct i2c_client *client)
{
	u8 wbuf[4];

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_LPWG_DEBUG_ENABLE;
	wbuf[2] = LPWG_DEBUG_ENABLE;

	if (Mit300_I2C_Write(client, wbuf, 3)) {
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

	if (Mit300_I2C_Write(client, wbuf, 3)) {
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

	if (Mit300_I2C_Write(client, wbuf, 3)) {
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
		mip_lpwg_config(client);
		mip_lpwg_config_knock_on(client);
		mip_lpwg_debug_enable(client);
		mip_lpwg_start(client);
		if (ts->currState == STATE_OFF)
			mip_lpwg_enable_sensing(client, 1);
		break;

	case STATE_KNOCK_ON_CODE:
		mip_lpwg_config(client);
		mip_lpwg_config_knock_on(client);
		mip_lpwg_config_knock_code(client);
		mip_lpwg_debug_enable(client);
		mip_lpwg_start(client);
		if (ts->currState == STATE_OFF)
			mip_lpwg_enable_sensing(client, 1);
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
	int intensityStatus = 0;
	struct i2c_client *client = Touch_Get_I2C_Handle();

	if (pDriverData == NULL)
		TOUCH_ERR("failed to get pDriverData for self diagnosis\n");

	TOUCH_FUNC();

	/* rawdata check */
	ret = MIT300_GetTestResult(client, buf, &intensityStatus, RAW_DATA_SHOW);
	if (ret < 0) {
		TOUCH_ERR("failed to get intensity data\n");
		ret = sprintf(buf, "failed to get intensity data\n");
	}

	return ret;
}

static ssize_t show_intensity(TouchDriverData *pDriverData, char *buf)
{
	int ret = 0;
	int intensityStatus = 0;
	struct i2c_client *client = Touch_Get_I2C_Handle();

	if (pDriverData == NULL)
		TOUCH_ERR("failed to get pDriverData for self diagnosis\n");

	TOUCH_FUNC();

	/* intensity check */
	ret = MIT300_GetTestResult(client, buf, &intensityStatus, INTENSITY_SHOW);
	if (ret < 0) {
		TOUCH_ERR("failed to get intensity data\n");
		ret = sprintf(buf, "failed to get intensity data\n");
	}

	return ret;
}

static LGE_TOUCH_ATTR(intensity, S_IRUGO | S_IWUSR, show_intensity, NULL);
static LGE_TOUCH_ATTR(rawdata, S_IRUGO | S_IWUSR, show_rawdata, NULL);

static struct attribute *MIT300_attribute_list[] = {
	&lge_touch_attr_intensity.attr,
	&lge_touch_attr_rawdata.attr,
	NULL,
};

static int MIT300_Initialize(TouchDriverData *pDriverData)
{
	struct i2c_client *client = Touch_Get_I2C_Handle();

	TOUCH_FUNC();

	/* IMPLEMENT : Device initialization at Booting */
	ts = devm_kzalloc(&client->dev, sizeof(struct melfas_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		TOUCH_ERR("failed to allocate memory for device driver data\n");
		return TOUCH_FAIL;
	}

	ts->client = client;

	return TOUCH_SUCCESS;
}

void MIT300_Reset(int status, int delay)
{
	if (!status)
		TouchDisableIrq();

	TouchSetGpioReset(status);
	if (delay <= 0 || delay > 1000) {
		TOUCH_LOG("%s exeeds limit %d\n", __func__, delay);
		return;
	}

	if (delay <= 20)
		mdelay(delay);
	else
		msleep(delay);

	if (status)
		TouchEnableIrq();
}

static void MIT300_Reset_Dummy(void)
{

}

static int MIT300_InitRegister(void)
{
	/* IMPLEMENT : Register initialization after reset */

	return TOUCH_SUCCESS;
}

static int MIT300_InterruptHandler(TouchReadData *pData)
{
	TouchFingerData *pFingerData = NULL;
	u8 i = 0;
	u8 wbuf[8] = { 0 };
	u8 rbuf[256] = { 0 };
	u32 packet_size = 0;
	u8 packet_type = 0;
	u8 alert_type = 0;
	u8 index = 0;
	u8 state = 0;
	struct i2c_client *client = Touch_Get_I2C_Handle();

	pData->type = DATA_UNKNOWN;
	pData->count = 0;

	if (LPWG_DEBUG_ENABLE == 0
	    && (ts->currState == STATE_KNOCK_ON_ONLY || ts->currState == STATE_KNOCK_ON_CODE)) {
		if (mip_i2c_dummy(client, wbuf, 2) == TOUCH_FAIL) {
			TOUCH_ERR("Fail to send dummy packet\n");
			return TOUCH_FAIL;
		}
	}
	/* Read packet info */
	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_PACKET_INFO;
	if (Mit300_I2C_Read(client, wbuf, 2, rbuf, 1)) {
		TOUCH_ERR("Read packet info\n");
		return TOUCH_FAIL;
	}

	packet_size = (rbuf[0] & 0x7F);
	packet_type = ((rbuf[0] >> 7) & 0x1);

	if (packet_size == 0)
		return TOUCH_SUCCESS;

	/* Read packet data */
	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_PACKET_DATA;
	if (Mit300_I2C_Read(client, wbuf, 2, rbuf, packet_size)) {
		TOUCH_ERR("Read packet data\n");
		return TOUCH_FAIL;
	}
	/* Event handler */
	if (packet_type == 0) {	/* Touch event */
		for (i = 0; i < packet_size; i += 6) {
			u8 *tmp = &rbuf[i];

			if ((tmp[0] & MIP_EVENT_INPUT_SCREEN) == 0) {
				TOUCH_LOG("use sofrware key\n");
				continue;
			}

			index = (tmp[0] & 0xf) - 1;
			state = (tmp[0] & 0x80) ? 1 : 0;

			if ((index < 0) || (index > MAX_NUM_OF_FINGERS - 1)) {
				TOUCH_ERR("invalid touch index (%d)\n", index);
				return TOUCH_FAIL;
			}

			pData->type = DATA_FINGER;

			if ((tmp[0] & MIP_EVENT_INPUT_PALM) >> 4) {
				if (state)
					TOUCH_LOG("Palm detected : %d\n", tmp[4]);
				else
					TOUCH_LOG("Palm released : %d\n", tmp[4]);

				return TOUCH_SUCCESS;
			}

			if (state) {
				pFingerData = &pData->fingerData[index];
				pFingerData->id = index;
				pFingerData->x = tmp[2] | ((tmp[1] & 0x0f) << 8);
				pFingerData->y = tmp[3] | ((tmp[1] & 0xf0) << 4);
				pFingerData->width_major = tmp[5];
				pFingerData->width_minor = 0;
				pFingerData->orientation = 0;
				pFingerData->pressure = tmp[4];
				if (tmp[4] < 1)
					pFingerData->pressure = 1;
				else if (tmp[4] > 255 - 1)
					pFingerData->pressure = 255 - 1;

				pData->count++;
				pFingerData->status = FINGER_PRESSED;
			} else {
				pFingerData = &pData->fingerData[index];
				pFingerData->id = index;
				pFingerData->status = FINGER_RELEASED;
			}
		}
	} else {
		/* Alert event */
		alert_type = rbuf[0];

		if (alert_type == MIP_ALERT_ESD) {
			/* ESD detection */
			TOUCH_LOG("ESD Detected!\n");
		} else if (alert_type == MIP_ALERT_WAKEUP) {
			/* Wake-up gesture */
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
				if (Mit300_I2C_Write(client, wbuf, 3)) {
					TOUCH_ERR("mip_i2c_write failed\n");
					return TOUCH_FAIL;
				}
			}
		} else if (alert_type == MIP_ALERT_F1) {
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
				if (Mit300_I2C_Write(client, wbuf, 3)) {
					TOUCH_ERR("mip_i2c_write failed\n");
					return TOUCH_FAIL;
				}
			}
		} else {
			TOUCH_LOG("Unknown alert type [%d]\n", alert_type);
		}
	}

	return TOUCH_SUCCESS;

}

static int MIT300_ReadIcFirmwareInfo(TouchFirmwareInfo *pFwInfo)
{
	u8 wbuf[2] = { 0, };
	u8 version[2] = { 0, };
	int ret = 0;
	struct i2c_client *client = Touch_Get_I2C_Handle();

	TOUCH_FUNC();

	/* IMPLEMENT : read IC firmware information function */
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_VERSION_CUSTOM;

	ret = Mit300_I2C_Read(client, wbuf, 2, version, 2);
	if (ret == TOUCH_FAIL)
		return TOUCH_FAIL;

	pFwInfo->moduleMakerID = 0;
	pFwInfo->moduleVersion = 0;
	pFwInfo->modelID = 0;
	pFwInfo->isOfficial = version[1];
	pFwInfo->version = version[0];

	TOUCH_LOG("IC F/W Version = v%X.%02X ( %s )\n", version[1], version[0],
		  pFwInfo->isOfficial ? "Official Release" : "Test Release");

	return TOUCH_SUCCESS;
}

static int MIT300_GetBinFirmwareInfo(char *pFilename, TouchFirmwareInfo *pFwInfo)
{
	int ret = 0;
	const struct firmware *fw = NULL;
	u8 version[2] = { 0, };
	u8 *pFwFilename = NULL;
	struct i2c_client *client = Touch_Get_I2C_Handle();

	TOUCH_FUNC();

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

	mip_bin_fw_version(ts, fw->data, fw->size, version);

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

static int MIT300_UpdateFirmware(char *pFilename)
{
	int ret = 0;
	char *pFwFilename = NULL;
	const struct firmware *fw = NULL;
	struct i2c_client *client = Touch_Get_I2C_Handle();

	TOUCH_FUNC();

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

	ret = mip_flash_fw(ts, fw->data, fw->size, false, true);
	if (ret < fw_err_none)
		return TOUCH_FAIL;

	release_firmware(fw);

	return TOUCH_SUCCESS;
}

static int MIT300_SetLpwgMode(TouchState newState, LpwgSetting *pLpwgSetting)
{
	int ret = TOUCH_SUCCESS;
	struct i2c_client *client = Touch_Get_I2C_Handle();

	TOUCH_FUNC();

	memcpy(&ts->lpwgSetting, pLpwgSetting, sizeof(LpwgSetting));

	if (ts->currState == newState) {
		TOUCH_LOG("device state is same as driver requested\n");
		return TOUCH_SUCCESS;
	}

	if ((newState < STATE_NORMAL) && (newState > STATE_KNOCK_ON_CODE)) {
		TOUCH_LOG("invalid request state ( state = %d )\n", newState);
		return TOUCH_FAIL;
	}

	ret = lpwg_control(client, newState);
	if (ret == TOUCH_FAIL) {
		TOUCH_ERR("failed to set lpwg mode in device\n");
		return TOUCH_FAIL;
	}

	if (ret == TOUCH_SUCCESS)
		ts->currState = newState;

	switch (newState) {
	case STATE_NORMAL:
		TOUCH_LOG("device was set to NORMAL\n");
		break;
	case STATE_OFF:
		TOUCH_LOG("device was set to OFF\n");
		break;
	case STATE_KNOCK_ON_ONLY:
		TOUCH_LOG("device was set to KNOCK_ON_ONLY\n");
		break;
	case STATE_KNOCK_ON_CODE:
		TOUCH_LOG("device was set to KNOCK_ON_CODE\n");
		break;
	default:
		TOUCH_LOG("impossilbe state ( state = %d )\n", newState);
		ret = TOUCH_FAIL;
		break;
	}

	return TOUCH_SUCCESS;
}

static int MIT300_DoSelfDiagnosis(int *pRawStatus, int *pChannelStatus, char *pBuf, int bufSize,
				  int *pDataLen)
{
	/* CAUTION : be careful not to exceed buffer size */
	char *sd_path = "/mnt/sdcard/touch_self_test.txt";
	int ret = 0;
	int deltaStatus = 0;
	int jitterStatus = 0;
	struct i2c_client *client = Touch_Get_I2C_Handle();

	memset(pBuf, 0, bufSize);
	*pDataLen = 0;
	TOUCH_FUNC();
	/* CAUTION : be careful not to exceed buffer size */

	/* IMPLEMENT : self-diagnosis function */

	*pRawStatus = TOUCH_SUCCESS;
	*pChannelStatus = TOUCH_SUCCESS;
	deltaStatus = TOUCH_SUCCESS;
	jitterStatus = TOUCH_SUCCESS;

	MIT300_WriteFile(sd_path, pBuf, 1);
	msleep(30);

	/* open short check */
	ret = MIT300_GetTestResult(client, pBuf, pChannelStatus, OPENSHORT_SHOW);
	if (ret < 0) {
		TOUCH_ERR("failed to get open short data\n");
		memset(pBuf, 0, bufSize);
		*pChannelStatus = TOUCH_FAIL;
		goto error;
	}
	MIT300_WriteFile(sd_path, pBuf, 0);
	msleep(30);
	memset(pBuf, 0, bufSize);

	/* open short 2 check (MUX) */
	ret = MIT300_GetTestResult(client, pBuf, pChannelStatus, MUXSHORT_SHOW);
	if (ret < 0) {
		TOUCH_ERR("failed to get open short (mux) data\n");
		memset(pBuf, 0, bufSize);
		*pChannelStatus = TOUCH_FAIL;
		goto error;
	}
	MIT300_WriteFile(sd_path, pBuf, 0);
	msleep(30);
	memset(pBuf, 0, bufSize);

	/* cm_delta check */
	ret = MIT300_GetTestResult(client, pBuf, &deltaStatus, DELTA_SHOW);
	if (ret < 0) {
		TOUCH_ERR("failed to get delta data\n");
		memset(pBuf, 0, bufSize);
		deltaStatus = TOUCH_FAIL;
		goto error;
	}
	MIT300_WriteFile(sd_path, pBuf, 0);
	msleep(30);
	memset(pBuf, 0, bufSize);

	/* cm_jitter check */
	ret = MIT300_GetTestResult(client, pBuf, &jitterStatus, JITTER_SHOW);
	if (ret < 0) {
		TOUCH_ERR("failed to get jitter data\n");
		memset(pBuf, 0, bufSize);
		jitterStatus = TOUCH_FAIL;
		goto error;
	}
	MIT300_WriteFile(sd_path, pBuf, 0);
	msleep(30);
	memset(pBuf, 0, bufSize);

	/* raw data check */
	ret = MIT300_GetTestResult(client, pBuf, pRawStatus, RAW_DATA_SHOW);
	if (ret < 0) {
		TOUCH_ERR("failed to get raw data\n");
		memset(pBuf, 0, bufSize);
		*pRawStatus = TOUCH_FAIL;
		goto error;
	}
	MIT300_WriteFile(sd_path, pBuf, 0);
	msleep(30);
	memset(pBuf, 0, bufSize);

	ret = sprintf(pBuf, "%s", "======ADDITIONAL======\n");
	ret +=
	    sprintf(pBuf + ret, "Delta Test: %s",
		    (deltaStatus == TOUCH_SUCCESS) ? "Pass\n" : "Fail\n");
	ret +=
	    sprintf(pBuf + ret, "Jitter Test: %s",
		    (jitterStatus == TOUCH_SUCCESS) ? "Pass\n" : "Fail\n");
	*pDataLen = ret;

	return TOUCH_SUCCESS;
error:
	return TOUCH_FAIL;
}

static void MIT300_PowerOn(int isOn)
{

}

static void MIT300_ClearInterrupt(void)
{

}

static void MIT300_NotifyHandler(TouchNotify notify, int data)
{

}

TouchDeviceControlFunction MIT300_Func = {
	.Power = MIT300_PowerOn,
	.Initialize = MIT300_Initialize,
	.Reset = MIT300_Reset_Dummy,
	.InitRegister = MIT300_InitRegister,
	.ClearInterrupt = MIT300_ClearInterrupt,
	.InterruptHandler = MIT300_InterruptHandler,
	.ReadIcFirmwareInfo = MIT300_ReadIcFirmwareInfo,
	.GetBinFirmwareInfo = MIT300_GetBinFirmwareInfo,
	.UpdateFirmware = MIT300_UpdateFirmware,
	.SetLpwgMode = MIT300_SetLpwgMode,
	.DoSelfDiagnosis = MIT300_DoSelfDiagnosis,
	.device_attribute_list = MIT300_attribute_list,
	.NotifyHandler = MIT300_NotifyHandler,
};


/* End Of File */
