// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>

#ifdef CONFIG_OF
/* device tree */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#ifdef CONFIG_MTK_CCU
#include "ccu_imgsensor_if.h"
#endif

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_camera_feature.h"
#include "kd_imgsensor_errcode.h"

#include "imgsensor_cfg_table.h"
#include "imgsensor_sensor_list.h"
#include "imgsensor_hw.h"
#include "imgsensor_i2c.h"
#include "imgsensor_proc.h"
#ifdef IMGSENSOR_OC_ENABLE
#include "imgsensor_oc.h"
#endif
#include "imgsensor.h"

#ifdef SENINF_N3D_SUPPORT
#include "n3d_fsync/n3d_if.h"
#endif

#if defined(CONFIG_MTK_CAM_SECURE_I2C)
#include "imgsensor_ca.h"
#endif

static DEFINE_MUTEX(gimgsensor_mutex);
static DEFINE_MUTEX(gimgsensor_open_mutex);

struct IMGSENSOR gimgsensor;
MUINT32 last_id;

/******************************************************************************
 * Profiling
 ******************************************************************************/
#define IMGSENSOR_PROF 1
#if IMGSENSOR_PROF
void IMGSENSOR_PROFILE_INIT(struct timeval *ptv)
{
	do_gettimeofday(ptv);
}

void IMGSENSOR_PROFILE(struct timeval *ptv, char *tag)
{
	struct timeval tv;
	unsigned long  time_interval;

	do_gettimeofday(&tv);
	time_interval =
	(tv.tv_sec - ptv->tv_sec) * 1000000 + (tv.tv_usec - ptv->tv_usec);

	PK_DBG("[%s]Profile = %lu us\n", tag, time_interval);
}

#else
void IMGSENSOR_PROFILE_INIT(struct timeval *ptv)
{
}

void IMGSENSOR_PROFILE(struct timeval *ptv, char *tag)
{
}
#endif

/******************************************************************************
 * sensor function adapter
 ******************************************************************************/
#define IMGSENSOR_FUNCTION_ENTRY()	/*PK_INFO("[%s]:E\n",__FUNCTION__) */
#define IMGSENSOR_FUNCTION_EXIT()	/*PK_INFO("[%s]:X\n",__FUNCTION__) */

struct IMGSENSOR_SENSOR
*imgsensor_sensor_get_inst(enum IMGSENSOR_SENSOR_IDX idx)
{
	if (idx < IMGSENSOR_SENSOR_IDX_MIN_NUM ||
		idx >= IMGSENSOR_SENSOR_IDX_MAX_NUM)
		return NULL;
	else
		return &gimgsensor.sensor[idx];
}

static void imgsensor_mutex_init(struct IMGSENSOR_SENSOR_INST *psensor_inst)
{
	mutex_init(&psensor_inst->sensor_mutex);
}

static void imgsensor_mutex_lock(struct IMGSENSOR_SENSOR_INST *psensor_inst)
{
#ifdef IMGSENSOR_LEGACY_COMPAT
	if (psensor_inst->status.arch) {
		mutex_lock(&psensor_inst->sensor_mutex);
	} else {
#ifdef SENSOR_PARALLEISM
		mutex_lock(&psensor_inst->sensor_mutex);
#else
		mutex_lock(&gimgsensor_mutex);
#endif
		imgsensor_i2c_set_device(&psensor_inst->i2c_cfg);
	}
#else
	mutex_lock(&psensor_inst->sensor_mutex);
#endif
}

static void imgsensor_mutex_unlock(struct IMGSENSOR_SENSOR_INST *psensor_inst)
{
#ifdef IMGSENSOR_LEGACY_COMPAT
	if (psensor_inst->status.arch)
		mutex_unlock(&psensor_inst->sensor_mutex);
	else {
#ifdef SENSOR_PARALLEISM
		imgsensor_i2c_set_device(NULL);
		mutex_unlock(&psensor_inst->sensor_mutex);
#else
		mutex_unlock(&gimgsensor_mutex);
#endif
	}
#else
	mutex_lock(&psensor_inst->sensor_mutex);
#endif
}

MINT32 imgsensor_sensor_open(struct IMGSENSOR_SENSOR *psensor)
{
	MINT32 ret = ERROR_NONE;
#if defined(CONFIG_MTK_CAM_SECURE_I2C)
	MINT32 ret_sec = ERROR_NONE;
	struct command_params c_params = {0};
#endif
	struct IMGSENSOR             *pimgsensor   = &gimgsensor;
	struct IMGSENSOR_SENSOR_INST *psensor_inst = &psensor->inst;
	struct SENSOR_FUNCTION_STRUCT *psensor_func =  psensor->pfunc;

#ifdef CONFIG_MTK_CCU
	struct ccu_sensor_info ccuSensorInfo;
	enum IMGSENSOR_SENSOR_IDX sensor_idx = psensor->inst.sensor_idx;
	struct i2c_client *pi2c_client = NULL;
#endif

	IMGSENSOR_FUNCTION_ENTRY();

	if (psensor_func && psensor_func->SensorOpen && psensor_inst) {

		/* turn on power */
		IMGSENSOR_PROFILE_INIT(&psensor_inst->profile_time);

		ret = imgsensor_hw_power(&pimgsensor->hw,
				psensor,
				IMGSENSOR_HW_POWER_STATUS_ON);

		if (ret != IMGSENSOR_RETURN_SUCCESS) {
			PK_PR_ERR("[%s]", __func__);
			return ret;
		}

		IMGSENSOR_PROFILE(&psensor_inst->profile_time,
			"kdCISModulePowerOn");

		imgsensor_mutex_lock(psensor_inst);

		psensor_func->psensor_inst = psensor_inst;

#if defined(CONFIG_MTK_CAM_SECURE_I2C)
	PK_INFO("%s secure state %d", __func__,
		(int)(&gimgsensor)->imgsensor_sec_flag);
		if ((&gimgsensor)->imgsensor_sec_flag) {
			ret = imgsensor_ca_invoke_command(
				IMGSENSOR_TEE_CMD_OPEN, c_params, &ret_sec);

		} else {
#endif
			ret = psensor_func->SensorOpen();
#if defined(CONFIG_MTK_CAM_SECURE_I2C)
		}
#endif

		if (ret != ERROR_NONE) {
			imgsensor_hw_dump(&pimgsensor->hw);
			imgsensor_hw_power(&pimgsensor->hw,
				psensor,
				IMGSENSOR_HW_POWER_STATUS_OFF);
			PK_PR_ERR("SensorOpen fail");
		} else {
			psensor_inst->state = IMGSENSOR_STATE_OPEN;
		}

#ifdef IMGSENSOR_OC_ENABLE
		if (ret == ERROR_NONE)
			imgsensor_oc_interrupt(IMGSENSOR_HW_POWER_STATUS_ON);
#endif

#ifdef CONFIG_MTK_CCU
		ccuSensorInfo.slave_addr =
		    (psensor_inst->i2c_cfg.msg->addr << 1);
		ccuSensorInfo.sensor_name_string =
		    (char *)(psensor_inst->psensor_list->name);
		pi2c_client = psensor_inst->i2c_cfg.pinst->pi2c_client;
		if (pi2c_client)
			ccuSensorInfo.i2c_id = (((struct mt_i2c *)
				i2c_get_adapdata(pi2c_client->adapter))->id);
		else
			ccuSensorInfo.i2c_id = -1;
		ccu_set_sensor_info(sensor_idx, &ccuSensorInfo);
#endif

		imgsensor_mutex_unlock(psensor_inst);

		IMGSENSOR_PROFILE(&psensor_inst->profile_time, "SensorOpen");
	}

	IMGSENSOR_FUNCTION_EXIT();

	return ret;
}

MUINT32
imgsensor_sensor_get_info(
		struct IMGSENSOR_SENSOR *psensor,
		MUINT32 ScenarioId,
		MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
		MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	MUINT32 ret = ERROR_NONE;
	struct IMGSENSOR_SENSOR_INST *psensor_inst = &psensor->inst;
	struct SENSOR_FUNCTION_STRUCT *psensor_func =  psensor->pfunc;

	IMGSENSOR_FUNCTION_ENTRY();

	if (psensor_func &&
	    psensor_func->SensorGetInfo &&
	    psensor_inst &&
	    pSensorInfo &&
	    pSensorConfigData) {

		imgsensor_mutex_lock(psensor_inst);

		psensor_func->psensor_inst = psensor_inst;

		ret = psensor_func->SensorGetInfo(
		    (enum MSDK_SCENARIO_ID_ENUM) (ScenarioId),
		    pSensorInfo,
		    pSensorConfigData);
		if (ret != ERROR_NONE)

			PK_PR_ERR("[%s] SensorGetInfo failed\n", __func__);

		imgsensor_mutex_unlock(psensor_inst);
	}

	IMGSENSOR_FUNCTION_EXIT();

	return ret;
}

MUINT32
imgsensor_sensor_get_resolution(
		struct IMGSENSOR_SENSOR *psensor,
		MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
	MUINT32 ret = ERROR_NONE;
	struct IMGSENSOR_SENSOR_INST *psensor_inst = &psensor->inst;
	struct SENSOR_FUNCTION_STRUCT *psensor_func =  psensor->pfunc;

	IMGSENSOR_FUNCTION_ENTRY();

	if (psensor_func &&
	    psensor_func->SensorGetResolution &&
	     psensor_inst) {

		imgsensor_mutex_lock(psensor_inst);

		psensor_func->psensor_inst = psensor_inst;

		ret = psensor_func->SensorGetResolution(pSensorResolution);
		if (ret != ERROR_NONE)
			PK_PR_ERR("[%s]\n", __func__);

		imgsensor_mutex_unlock(psensor_inst);
	}

	IMGSENSOR_FUNCTION_EXIT();

	return ret;
}

MUINT32
imgsensor_sensor_feature_control(
		struct IMGSENSOR_SENSOR *psensor,
		MSDK_SENSOR_FEATURE_ENUM FeatureId,
		MUINT8 *pFeaturePara,
		MUINT32 *pFeatureParaLen)
{
	MUINT32 ret = ERROR_NONE;
#if defined(CONFIG_MTK_CAM_SECURE_I2C)
	MINT32 ret_sec = ERROR_NONE;
	struct command_params c_params;
#endif
	struct IMGSENSOR_SENSOR_INST  *psensor_inst = &psensor->inst;
	struct SENSOR_FUNCTION_STRUCT *psensor_func =  psensor->pfunc;

	IMGSENSOR_FUNCTION_ENTRY();

	if (psensor_func &&
	    psensor_func->SensorFeatureControl &&
	    psensor_inst) {

		imgsensor_mutex_lock(psensor_inst);
		if (FeatureId == SENSOR_FEATURE_GET_TEMPERATURE_VALUE &&
		    psensor_inst->state == IMGSENSOR_STATE_CLOSE){
			//should do nothing in close stage
			imgsensor_mutex_unlock(psensor_inst);
			return ret;
		}
		psensor_func->psensor_inst = psensor_inst;
#if defined(CONFIG_MTK_CAM_SECURE_I2C)
	PK_INFO("%s secure state %d", __func__,
		(int)(&gimgsensor)->imgsensor_sec_flag);
	if ((&gimgsensor)->imgsensor_sec_flag) {

		c_params.param0 = (void *)FeatureId;
		c_params.param1 = (void *)pFeaturePara;
		c_params.param2 = (void *)pFeatureParaLen;
		ret = imgsensor_ca_invoke_command(
			IMGSENSOR_TEE_CMD_FEATURE_CONTROL, c_params, &ret_sec);

	} else {
#endif
		ret = psensor_func->SensorFeatureControl(
			FeatureId, pFeaturePara, pFeatureParaLen);
#if defined(CONFIG_MTK_CAM_SECURE_I2C)
	}
#endif
		if (ret != ERROR_NONE)
			PK_PR_ERR("[%s]\n", __func__);

		imgsensor_mutex_unlock(psensor_inst);
	}

	IMGSENSOR_FUNCTION_EXIT();

	return ret;
}

MUINT32
imgsensor_sensor_control(
	struct IMGSENSOR_SENSOR *psensor,
	enum MSDK_SCENARIO_ID_ENUM ScenarioId)
{
	MUINT32 ret = ERROR_NONE;
#if defined(CONFIG_MTK_CAM_SECURE_I2C)
	MINT32 ret_sec = ERROR_NONE;
	struct command_params c_params;
#endif
	struct IMGSENSOR_SENSOR_INST *psensor_inst = &psensor->inst;
	struct SENSOR_FUNCTION_STRUCT *psensor_func =  psensor->pfunc;

	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT image_window;
	MSDK_SENSOR_CONFIG_STRUCT sensor_config_data;

	IMGSENSOR_FUNCTION_ENTRY();

	if (psensor_func &&
	    psensor_func->SensorControl &&
	    psensor_inst) {

		IMGSENSOR_PROFILE_INIT(&psensor_inst->profile_time);

		imgsensor_mutex_lock(psensor_inst);

		psensor_func->psensor_inst = psensor_inst;
		psensor_func->ScenarioId = ScenarioId;

#if defined(CONFIG_MTK_CAM_SECURE_I2C)
	PK_INFO("%s secure state %d", __func__,
		(int)(&gimgsensor)->imgsensor_sec_flag);
	if ((&gimgsensor)->imgsensor_sec_flag) {
		c_params.param0 = (void *)ScenarioId;
		c_params.param1 = (void *)&image_window;
		c_params.param2 = (void *)&sensor_config_data;
		ret = imgsensor_ca_invoke_command(
			IMGSENSOR_TEE_CMD_CONTROL, c_params, &ret_sec);

	} else {
#endif
		ret = psensor_func->SensorControl(
			ScenarioId, &image_window, &sensor_config_data);
#if defined(CONFIG_MTK_CAM_SECURE_I2C)
	}
#endif
		if (ret != ERROR_NONE)
			PK_PR_ERR("[%s]\n", __func__);

		imgsensor_mutex_unlock(psensor_inst);

		IMGSENSOR_PROFILE(
		    &psensor_inst->profile_time,
		    "SensorControl");
	}

	IMGSENSOR_FUNCTION_EXIT();

	return ret;
}

MINT32 imgsensor_sensor_close(struct IMGSENSOR_SENSOR *psensor)
{
	MINT32 ret = ERROR_NONE;
#if defined(CONFIG_MTK_CAM_SECURE_I2C)
	MINT32 ret_sec = ERROR_NONE;
	struct command_params c_params = {0};
#endif
	struct IMGSENSOR *pimgsensor = &gimgsensor;
	struct IMGSENSOR_SENSOR_INST  *psensor_inst = &psensor->inst;
	struct SENSOR_FUNCTION_STRUCT *psensor_func =  psensor->pfunc;

	IMGSENSOR_FUNCTION_ENTRY();

	if (psensor_func &&
	    psensor_func->SensorClose &&
	    psensor_inst) {

		IMGSENSOR_PROFILE_INIT(&psensor_inst->profile_time);

		imgsensor_mutex_lock(psensor_inst);

#ifdef IMGSENSOR_OC_ENABLE
		imgsensor_oc_interrupt(IMGSENSOR_HW_POWER_STATUS_OFF);
#endif

		psensor_func->psensor_inst = psensor_inst;
#if defined(CONFIG_MTK_CAM_SECURE_I2C)
		PK_INFO("%s secure state %d", __func__,
			(int)(&gimgsensor)->imgsensor_sec_flag);
		if ((&gimgsensor)->imgsensor_sec_flag) {
			ret = imgsensor_ca_invoke_command(
				IMGSENSOR_TEE_CMD_CLOSE, c_params, &ret_sec);
		} else {
#endif
			ret = psensor_func->SensorClose();
#if defined(CONFIG_MTK_CAM_SECURE_I2C)
		}
#endif
		if (ret != ERROR_NONE) {
			PK_PR_ERR("[%s]", __func__);
		} else {
			imgsensor_hw_power(&pimgsensor->hw,
				psensor,
				IMGSENSOR_HW_POWER_STATUS_OFF);

			psensor_inst->state = IMGSENSOR_STATE_CLOSE;
		}

		imgsensor_mutex_unlock(psensor_inst);

		IMGSENSOR_PROFILE(&psensor_inst->profile_time, "SensorClose");
	}

	IMGSENSOR_FUNCTION_EXIT();

	return ret;
}

static void imgsensor_init_sensor_list(void)
{
	unsigned int i = 0;
	int ret = 0;
	struct IMGSENSOR             *pimgsensor   = &gimgsensor;
	struct IMGSENSOR_SENSOR_LIST *psensor_list =  gimgsensor_sensor_list;
	const char *penable_sensor;
	struct device_node *of_node
		= of_find_compatible_node(NULL, NULL, "mediatek,imgsensor");

	ret = of_property_read_string(of_node, "cust-sensor", &penable_sensor);
	if (ret < 0) {
		PK_DBG("Property cust-sensor not defined\n");
		while (psensor_list->id && i < MAX_NUM_OF_SUPPORT_SENSOR) {
			pimgsensor->psensor_list[i] = psensor_list;
			i++;
			psensor_list++;
		}
	} else {
		PK_DBG("Customizedsensors: %s\n", penable_sensor);
		while (psensor_list->id && i < MAX_NUM_OF_SUPPORT_SENSOR) {
			if (strstr(penable_sensor, psensor_list->name)) {
				pimgsensor->psensor_list[i] = psensor_list;
				i++;
			}
			psensor_list++;
		}
	}
}

/******************************************************************************
 * imgsensor_check_is_alive
 ******************************************************************************/
static inline int imgsensor_check_is_alive(struct IMGSENSOR_SENSOR *psensor)
{
	MINT32 ret = ERROR_NONE;
	UINT32 err = 0;
	MUINT32 sensorID = 0;
	MUINT32 retLen = sizeof(MUINT32);
	struct IMGSENSOR *pimgsensor = &gimgsensor;
	struct IMGSENSOR_SENSOR_INST *psensor_inst = &psensor->inst;

	IMGSENSOR_PROFILE_INIT(&psensor_inst->profile_time);
	ret = imgsensor_hw_power(&pimgsensor->hw,
			psensor,
			IMGSENSOR_HW_POWER_STATUS_ON);

	if (ret != IMGSENSOR_RETURN_SUCCESS)
		return ERROR_SENSOR_CONNECT_FAIL;

	imgsensor_sensor_feature_control(psensor,
					 SENSOR_FEATURE_CHECK_SENSOR_ID,
					 (MUINT8 *) &sensorID, &retLen);

	/* not implement this feature ID */
	if (sensorID == 0 || sensorID == 0xFFFFFFFF) {
		PK_DBG("Fail to get sensor ID %x\n", sensorID);
		err = ERROR_SENSOR_CONNECT_FAIL;
	} else {
		PK_DBG("Sensor found ID = 0x%x\n", sensorID);
		err = ERROR_NONE;
	}

	imgsensor_hw_power(&pimgsensor->hw,
			psensor,
			IMGSENSOR_HW_POWER_STATUS_OFF);
	IMGSENSOR_PROFILE(&psensor_inst->profile_time, "CheckIsAlive");

	return err ? -EIO : err;
}

/******************************************************************************
 * imgsensor_set_driver
 ******************************************************************************/
int imgsensor_set_driver(struct IMGSENSOR_SENSOR *psensor)
{
	int ret = -EIO;
	unsigned int i = 0;
	struct IMGSENSOR             *pimgsensor   = &gimgsensor;
	struct IMGSENSOR_SENSOR_INST *psensor_inst = &psensor->inst;

	imgsensor_mutex_init(psensor_inst);
	imgsensor_i2c_init(&psensor_inst->i2c_cfg,
	imgsensor_custom_config[
	(unsigned int)psensor_inst->sensor_idx].i2c_dev);
	imgsensor_i2c_filter_msg(&psensor_inst->i2c_cfg, true);

	while (i < MAX_NUM_OF_SUPPORT_SENSOR && pimgsensor->psensor_list[i]) {
		if (pimgsensor->psensor_list[i]->init) {
			pimgsensor->psensor_list[i]->init(&psensor->pfunc);

			if (psensor->pfunc) {
				/* get sensor name */
				psensor_inst->psensor_list
					= pimgsensor->psensor_list[i];
#ifdef IMGSENSOR_LEGACY_COMPAT
				psensor_inst->status.arch
					= psensor->pfunc->arch;
#endif

				if (!imgsensor_check_is_alive(psensor)) {
					PK_INFO(
					"[%s] :[%d][%s]\n",
					__func__,
					psensor_inst->sensor_idx,
					psensor_inst->psensor_list->name);
					ret = 0;
					break;
				}
			} else {
				PK_PR_ERR(
					"ERROR:NULL g_pInvokeSensorFunc[%d]\n",
					psensor_inst->sensor_idx);
			}
		} else {
			PK_PR_ERR("ERROR:NULL sensor list\n");
		}

		i++;
	}

	imgsensor_i2c_filter_msg(&psensor_inst->i2c_cfg, false);

	return ret;
}

MUINT32 Get_Camera_Temperature(
	enum CAMERA_DUAL_CAMERA_SENSOR_ENUM indexDual,
	MUINT8 *valid,
	MINT32 *temp)
{
	MUINT32 ret = IMGSENSOR_RETURN_SUCCESS;
	MUINT32 FeatureParaLen = 0;
	struct IMGSENSOR_SENSOR *psensor =
	    imgsensor_sensor_get_inst(IMGSENSOR_SENSOR_IDX_MAP(indexDual));
	struct IMGSENSOR_SENSOR_INST *psensor_inst;

	if (valid == NULL || temp == NULL || psensor == NULL)
		return IMGSENSOR_RETURN_ERROR;

	*valid = SENSOR_TEMPERATURE_NOT_SUPPORT_THERMAL |
			SENSOR_TEMPERATURE_NOT_POWER_ON;
	*temp = 0;

	psensor_inst = &psensor->inst;

	FeatureParaLen = sizeof(MUINT32);

	/* Sensor is not in close state,
	 * where in close state the temperature is not valid
	 */
	if (psensor_inst->state != IMGSENSOR_STATE_CLOSE) {
		ret = imgsensor_sensor_feature_control(psensor,
					SENSOR_FEATURE_GET_TEMPERATURE_VALUE,
					(MUINT8 *) temp,
					(MUINT32 *) &FeatureParaLen);

		PK_DBG("indexDual(%d), temperature(%d)\n", indexDual, *temp);

		*valid &= ~SENSOR_TEMPERATURE_NOT_POWER_ON;

		if (*temp != 0) {
			*valid |= SENSOR_TEMPERATURE_VALID;
			*valid &= ~SENSOR_TEMPERATURE_NOT_SUPPORT_THERMAL;
		}
	}

	return ret;
}
EXPORT_SYMBOL(Get_Camera_Temperature);

static inline int adopt_CAMERA_HW_GetInfo2(void *pBuf)
{
	unsigned int i = 0;
	int ret = 0;
	struct IMAGESENSOR_GETINFO_STRUCT *pSensorGetInfo;
	struct IMGSENSOR_SENSOR *psensor;

	struct ACDK_SENSOR_INFO_STRUCT info;
	struct ACDK_SENSOR_CONFIG_STRUCT config;
	struct ACDK_SENSOR_RESOLUTION_INFO_STRUCT sensor_resolution;

	MUINT16 *pSensorGrabStart = &info.SensorGrabStartX_PRV;
	MUINT8 *pMIPIDataLowPwr2HSSettleDelay =
			&info.MIPIDataLowPwr2HSSettleDelayM0;
	MUINT8 *pDPCMType = &info.IMGSENSOR_DPCM_TYPE_PRE;
	char *pmtk_ccm_name;

	memset(&info, 0,
			sizeof(struct ACDK_SENSOR_INFO_STRUCT));
	memset(&sensor_resolution,
			0,
			sizeof(struct ACDK_SENSOR_RESOLUTION_INFO_STRUCT));

	pSensorGetInfo = (struct IMAGESENSOR_GETINFO_STRUCT *) pBuf;
	if (pSensorGetInfo == NULL ||
			pSensorGetInfo->pInfo == NULL ||
			pSensorGetInfo->pSensorResolution == NULL) {
		PK_DBG("[%s] NULL arg.\n", __func__);
		return -EFAULT;
	}

	psensor = imgsensor_sensor_get_inst(pSensorGetInfo->SensorId);
	if (psensor == NULL) {
		PK_DBG("[%s] NULL psensor.\n", __func__);
		return -EFAULT;
	}

	PK_DBG("[%s]Entry%d\n", __func__, pSensorGetInfo->SensorId);

	for (i = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
			i < MSDK_SCENARIO_ID_MAX;
			i++) {
		imgsensor_sensor_get_info(psensor, i, &info, &config);

		pSensorGrabStart[i * 2] = info.SensorGrabStartX;
		pSensorGrabStart[i * 2 + 1] = info.SensorGrabStartY;

		if (i < MSDK_SCENARIO_ID_CUSTOM1) {
			pMIPIDataLowPwr2HSSettleDelay[i] =
				info.MIPIDataLowPwr2HighSpeedSettleDelayCount;
			pDPCMType[i] = info.DPCM_INFO;
		}
	}

	if (copy_to_user((void __user *)(pSensorGetInfo->pInfo),
			(void *)(&info),
			sizeof(struct ACDK_SENSOR_INFO_STRUCT))) {

		PK_DBG("[CAMERA_HW][info] ioctl copy to user failed\n");
		return -EFAULT;
	}

	/* Step2 : Get Resolution */
	imgsensor_sensor_get_resolution(psensor, &sensor_resolution);

	PK_DBG("[CAMERA_HW][Pre]w=0x%x, h = 0x%x\n",
			sensor_resolution.SensorPreviewWidth,
			sensor_resolution.SensorPreviewHeight);
	PK_DBG("[CAMERA_HW][Full]w=0x%x, h = 0x%x\n",
			sensor_resolution.SensorFullWidth,
			sensor_resolution.SensorFullHeight);
	PK_DBG("[CAMERA_HW][VD]w=0x%x, h = 0x%x\n",
			sensor_resolution.SensorVideoWidth,
			sensor_resolution.SensorVideoHeight);

	if (pSensorGetInfo->SensorId <= last_id) {
		memset(mtk_ccm_name, 0, camera_info_size);
		PK_DBG("memset ok");
	}
	last_id = pSensorGetInfo->SensorId;

	/* Add info to proc: camera_info */
	pmtk_ccm_name = strchr(mtk_ccm_name, '\0');
	if (pmtk_ccm_name == NULL)
		return -EFAULT;
	ret = snprintf(
			pmtk_ccm_name,
			camera_info_size - (int)(pmtk_ccm_name - mtk_ccm_name),
			"\nCAM[%d]:%s;",
			psensor->inst.sensor_idx,
			psensor->inst.psensor_list->name);
	if (ret < 0)
		return ret;

	pmtk_ccm_name = strchr(mtk_ccm_name, '\0');
	if (pmtk_ccm_name == NULL)
		return -EFAULT;
	ret = snprintf(
			pmtk_ccm_name,
			camera_info_size - (int)(pmtk_ccm_name - mtk_ccm_name),
			"\nSensor ID = %x;",
			psensor->inst.psensor_list->id);
	if (ret < 0)
		return ret;

	pmtk_ccm_name = strchr(mtk_ccm_name, '\0');
	if (pmtk_ccm_name == NULL)
		return -EFAULT;
	ret = snprintf(
			pmtk_ccm_name,
			camera_info_size - (int)(pmtk_ccm_name - mtk_ccm_name),
			"\nPre: TgGrab_w,h,x_,y=%5d,%5d,%3d,%3d, delay_frm=%2d",
			sensor_resolution.SensorPreviewWidth,
			sensor_resolution.SensorPreviewHeight,
			info.SensorGrabStartX_PRV,
			info.SensorGrabStartY_PRV,
			info.PreviewDelayFrame);
	if (ret < 0)
		return ret;

	pmtk_ccm_name = strchr(mtk_ccm_name, '\0');
	if (pmtk_ccm_name == NULL)
		return -EFAULT;
	ret = snprintf(
			pmtk_ccm_name,
			camera_info_size - (int)(pmtk_ccm_name - mtk_ccm_name),
			"\nCap: TgGrab_w,h,x_,y=%5d,%5d,%3d,%3d, delay_frm=%2d",
			sensor_resolution.SensorFullWidth,
			sensor_resolution.SensorFullHeight,
			info.SensorGrabStartX_CAP,
			info.SensorGrabStartY_CAP,
			info.CaptureDelayFrame);
	if (ret < 0)
		return ret;

	pmtk_ccm_name = strchr(mtk_ccm_name, '\0');
	if (pmtk_ccm_name == NULL)
		return -EFAULT;
	ret = snprintf(
			pmtk_ccm_name,
			camera_info_size - (int)(pmtk_ccm_name - mtk_ccm_name),
			"\nVid: TgGrab_w,h,x_,y=%5d,%5d,%3d,%3d, delay_frm=%2d",
			sensor_resolution.SensorVideoWidth,
			sensor_resolution.SensorVideoHeight,
			info.SensorGrabStartX_VD,
			info.SensorGrabStartY_VD,
			info.VideoDelayFrame);
	if (ret < 0)
		return ret;

	pmtk_ccm_name = strchr(mtk_ccm_name, '\0');
	if (pmtk_ccm_name == NULL)
		return -EFAULT;
	ret = snprintf(
			pmtk_ccm_name,
			camera_info_size - (int)(pmtk_ccm_name - mtk_ccm_name),
			"\nHSV: TgGrab_w,h,x_,y=%5d,%5d,%3d,%3d, delay_frm=%2d",
			sensor_resolution.SensorHighSpeedVideoWidth,
			sensor_resolution.SensorHighSpeedVideoHeight,
			info.SensorGrabStartX_VD1,
			info.SensorGrabStartY_VD1,
			info.HighSpeedVideoDelayFrame);
	if (ret < 0)
		return ret;

	pmtk_ccm_name = strchr(mtk_ccm_name, '\0');
	if (pmtk_ccm_name == NULL)
		return -EFAULT;
	ret = snprintf(
			pmtk_ccm_name,
			camera_info_size - (int)(pmtk_ccm_name - mtk_ccm_name),
			"\nSLV: TgGrab_w,h,x_,y=%5d,%5d,%3d,%3d, delay_frm=%2d",
			sensor_resolution.SensorSlimVideoWidth,
			sensor_resolution.SensorSlimVideoHeight,
			info.SensorGrabStartX_VD2,
			info.SensorGrabStartY_VD2,
			info.SlimVideoDelayFrame);
	if (ret < 0)
		return ret;

	pmtk_ccm_name = strchr(mtk_ccm_name, '\0');
	if (pmtk_ccm_name == NULL)
		return -EFAULT;
	ret = snprintf(
			pmtk_ccm_name,
			camera_info_size - (int)(pmtk_ccm_name - mtk_ccm_name),
			"\nSeninf_Type(0:parallel,1:mipi,2:serial)=%d, output_format(0:B,1:Gb,2:Gr,3:R)=%2d",
			info.SensroInterfaceType,
			info.SensorOutputDataFormat);
	if (ret < 0)
		return ret;

	pmtk_ccm_name = strchr(mtk_ccm_name, '\0');
	if (pmtk_ccm_name == NULL)
		return -EFAULT;
	ret = snprintf(
			pmtk_ccm_name,
			camera_info_size - (int)(pmtk_ccm_name - mtk_ccm_name),
			"\nDriving_Current(0:2mA,1:4mA,2:6mA,3:8mA)=%d, mclk_freq=%2d, mipi_lane=%d",
			info.SensorDrivingCurrent,
			info.SensorClockFreq,
			info.SensorMIPILaneNumber + 1);
	if (ret < 0)
		return ret;

	pmtk_ccm_name = strchr(mtk_ccm_name, '\0');
	if (pmtk_ccm_name == NULL)
		return -EFAULT;
	ret = snprintf(
			pmtk_ccm_name,
			camera_info_size - (int)(pmtk_ccm_name - mtk_ccm_name),
			"\nPDAF_Support(0:No PD,1:PD RAW,2:VC(Full),3:VC(Bin),4:Dual Raw,5:Dual VC=%2d",
			info.PDAF_Support);
	if (ret < 0)
		return ret;

	pmtk_ccm_name = strchr(mtk_ccm_name, '\0');
	if (pmtk_ccm_name == NULL)
		return -EFAULT;
	ret = snprintf(
			pmtk_ccm_name,
			camera_info_size - (int)(pmtk_ccm_name - mtk_ccm_name),
			"\nHDR_Support(0:NO HDR,1: iHDR,2:mvHDR,3:zHDR)=%2d",
			info.HDR_Support);
	if (ret < 0)
		return ret;

	/* Resolution */
	if (copy_to_user((void __user *)(pSensorGetInfo->pSensorResolution),
			(void *)&sensor_resolution,
			sizeof(struct ACDK_SENSOR_RESOLUTION_INFO_STRUCT))) {

		PK_DBG("[CAMERA_HW][Resolution] ioctl copy to user failed\n");
		return -EFAULT;
	}

	return 0;
}


/******************************************************************************
 * adopt_CAMERA_HW_Control
 ******************************************************************************/
static inline int adopt_CAMERA_HW_Control(void *pBuf)
{
	int ret = 0;
	struct ACDK_SENSOR_CONTROL_STRUCT *pSensorCtrl;
	struct IMGSENSOR_SENSOR *psensor;

	pSensorCtrl = (struct ACDK_SENSOR_CONTROL_STRUCT *) pBuf;
	if (pSensorCtrl == NULL) {
		PK_PR_ERR("[%s] NULL arg.\n", __func__);
		return -EFAULT;
	}

	psensor = imgsensor_sensor_get_inst(pSensorCtrl->InvokeCamera);
	if (psensor == NULL) {
		PK_PR_ERR("[%s] NULL psensor.\n", __func__);
		return -EFAULT;
	}

	ret = imgsensor_sensor_control(psensor, pSensorCtrl->ScenarioId);

	return ret;
}

/******************************************************************************
 * adopt_CAMERA_HW_FeatureControl
 ******************************************************************************/
static inline int adopt_CAMERA_HW_FeatureControl(void *pBuf)
{
	struct ACDK_SENSOR_FEATURECONTROL_STRUCT *pFeatureCtrl;
	struct IMGSENSOR_SENSOR *psensor;
	unsigned int FeatureParaLen = 0;
	void *pFeaturePara = NULL;
	struct ACDK_KD_SENSOR_SYNC_STRUCT *pSensorSyncInfo = NULL;
	signed int ret = 0;
	struct SET_SENSOR_PATTERN_SOLID_COLOR *pData = NULL;
	unsigned long long *pFeaturePara_64 = NULL;
	void *usr_ptr = NULL;
	kal_uint32 buf_sz;

#ifdef CONFIG_MTK_CAM_SECURE_I2C
	struct command_params c_params;

	memset(&c_params, 0, sizeof(struct command_params));
#endif

	pFeatureCtrl = (struct ACDK_SENSOR_FEATURECONTROL_STRUCT *) pBuf;
	if (pFeatureCtrl == NULL) {
		PK_PR_ERR("NULL pFeatureCtrl\n");
		return -EFAULT;
	}

	psensor = imgsensor_sensor_get_inst(pFeatureCtrl->InvokeCamera);
	if (psensor == NULL) {
		PK_PR_ERR("NULL psensor.\n");
		return -EFAULT;
	}

	if (pFeatureCtrl->pFeatureParaLen != NULL &&
	    copy_from_user((void *)&FeatureParaLen,
			   (void *)pFeatureCtrl->pFeatureParaLen,
			   sizeof(unsigned int))) {
		PK_PR_ERR(" ioctl copy from user failed\n");
		return -EFAULT;
	}

	/* data size exam */
	if (FeatureParaLen > IMGSENSOR_FEATURE_PARA_LEN_MAX) {
		PK_PR_ERR("exceed data size limitation\n");
		return -EFAULT;
	}

	if (FeatureParaLen != 0 && pFeatureCtrl->pFeaturePara != NULL) {
		pFeaturePara = kmalloc(FeatureParaLen, GFP_KERNEL);
		if (pFeaturePara == NULL) {
			PK_PR_ERR(" ioctl allocate mem failed\n");
			return -ENOMEM;
		}
		memset(pFeaturePara, 0x0, FeatureParaLen);

		if (copy_from_user((void *)pFeaturePara,
				   (void *)pFeatureCtrl->pFeaturePara,
				   FeatureParaLen)) {
			kfree(pFeaturePara);
			PK_PR_ERR(
		"[CAMERA_HW][pFeaturePara] ioctl copy from user failed\n");
			return -EFAULT;
		}
	} else {
		PK_PR_ERR("Wrong FeatureParaLen or pFeaturePara: %d %p\n",
			FeatureParaLen, pFeatureCtrl->pFeaturePara);
		return -EFAULT;
	}

	/*in case that some structure are passed from user sapce by ptr */
	switch (pFeatureCtrl->FeatureId) {
	case SENSOR_FEATURE_SET_MCLK_DRIVE_CURRENT:
	{
		MUINT32 __current = (*(MUINT32 *)pFeaturePara);
		if (FeatureParaLen < 1 * sizeof(unsigned long long)) {
			PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
			kfree(pFeaturePara);
			return -EINVAL;
		}
		if (gimgsensor.mclk_set_drive_current != NULL)
			gimgsensor.mclk_set_drive_current(
			gimgsensor.hw.pdev[IMGSENSOR_HW_ID_MCLK]->pinstance,
				pFeatureCtrl->InvokeCamera,
				__current);
		else
			pr_debug(
				"%s, set drive current by pinctrl was not supported\n",
				__func__);

		break;
	}
#ifdef CONFIG_MTK_CAM_SECURE_I2C
	case SENSOR_FEATURE_OPEN_SECURE_SESSION:
		PK_INFO("SECURE_SENSOR_ID = %x\n",
			(int)psensor->inst.psensor_list->id);

		if (FeatureParaLen < 1 * sizeof(unsigned long long)) {
			PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
			kfree(pFeaturePara);
			return -EINVAL;
		}

		/* size : IMGSENSOR_SENSOR */
		c_params.param0 =
			(void *)(uintptr_t)(psensor->inst.psensor_list->id);

		imgsensor_ca_open();
		if ((imgsensor_ca_invoke_command(IMGSENSOR_TEE_CMD_SET_SENSOR,
		c_params, &ret) != 0) || (ret != 0)) {
			PK_DBG("Error!! set secure sensor_pfunc failed!");
			return ret;
		}
		break;
	case SENSOR_FEATURE_CLOSE_SECURE_SESSION:
		imgsensor_ca_close();
		break;
	case SENSOR_FEATURE_SET_AS_SECURE_DRIVER:
		if (FeatureParaLen < 1 * sizeof(unsigned long long)) {
			PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
			kfree(pFeaturePara);
			return -EINVAL;
		}
		(&gimgsensor)->imgsensor_sec_flag =
			(*(unsigned long long *)pFeaturePara);
		PK_INFO("debug: secure set as %d",
			(int)((&gimgsensor)->imgsensor_sec_flag));
		break;
#endif
	case SENSOR_FEATURE_SET_I2C_BUF_MODE_EN:
		if (FeatureParaLen < 1 * sizeof(unsigned long long)) {
			PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
			kfree(pFeaturePara);
			return -EINVAL;
		}
		ret = imgsensor_i2c_buffer_mode(
			(*(unsigned long long *)pFeaturePara));
		break;

	case SENSOR_FEATURE_OPEN:
		ret = imgsensor_sensor_open(psensor);
		break;

	case SENSOR_FEATURE_CLOSE:
		ret = imgsensor_sensor_close(psensor);
		/* reset the delay frame flag */
		break;

	case SENSOR_FEATURE_SET_DRIVER:
	{
		struct IMGSENSOR_SENSOR_LIST *psensor_list =
			(struct IMGSENSOR_SENSOR_LIST *)pFeaturePara;

		if (FeatureParaLen < 4 * sizeof(unsigned long long)) {
			PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
			kfree(pFeaturePara);
			return -EINVAL;
		}

		psensor->inst.sensor_idx = pFeatureCtrl->InvokeCamera;
		if (imgsensor_set_driver(psensor) != -EIO) {
			psensor_list->id = psensor->inst.psensor_list->id;
			memcpy(psensor_list->name,
			       psensor->inst.psensor_list->name,
			       32);
		}

		break;
	}

	case SENSOR_FEATURE_CHECK_IS_ALIVE:
		imgsensor_check_is_alive(psensor);
		break;

	case SENSOR_FEATURE_SET_SENSOR_SYNC:
	case SENSOR_FEATURE_SET_ESHUTTER_GAIN:
		PK_DBG("[kd_sensorlist]enter kdSetExpGain\n");
		/* keep the information to wait Vsync synchronize */
		pSensorSyncInfo =
			(struct ACDK_KD_SENSOR_SYNC_STRUCT *) pFeaturePara;

		FeatureParaLen = 2;

		imgsensor_sensor_feature_control(
			psensor,
			SENSOR_FEATURE_SET_ESHUTTER,
			(unsigned char *)&pSensorSyncInfo->u2SensorNewExpTime,
			(unsigned int *)&FeatureParaLen);

		imgsensor_sensor_feature_control(
			psensor,
			SENSOR_FEATURE_SET_GAIN,
			(unsigned char *)&pSensorSyncInfo->u2SensorNewGain,
			(unsigned int *) &FeatureParaLen);
		break;
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		{
			MUINT32 *pValue = NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;

			if (FeatureParaLen < 2 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}

			pValue = kmalloc(sizeof(MUINT32), GFP_KERNEL);
			if (pValue == NULL) {
				PK_PR_ERR(" ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}

			memset(pValue, 0x0, sizeof(MUINT32));
			*(pFeaturePara_64 + 1) = (uintptr_t) pValue;

			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

			*(pFeaturePara_64 + 1) = *pValue;
			kfree(pValue);
		}
		break;

	case SENSOR_FEATURE_GET_AE_AWB_LOCK_INFO:
	case SENSOR_FEATURE_AUTOTEST_CMD:
	{
		MUINT32 *pValue0 = NULL;
		MUINT32 *pValue1 = NULL;
		unsigned long long *pFeaturePara_64 =
			(unsigned long long *)pFeaturePara;

		if (FeatureParaLen < 2 * sizeof(unsigned long long)) {
			PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
			kfree(pFeaturePara);
			return -EINVAL;
		}

		pValue0 = kmalloc(sizeof(MUINT32), GFP_KERNEL);
		pValue1 = kmalloc(sizeof(MUINT32), GFP_KERNEL);

		if (pValue0 == NULL || pValue1 == NULL) {
			PK_PR_ERR(" ioctl allocate mem failed\n");
			kfree(pValue0);
			kfree(pValue1);
			kfree(pFeaturePara);
			return -ENOMEM;
		}
		memset(pValue1, 0x0, sizeof(MUINT32));
		memset(pValue0, 0x0, sizeof(MUINT32));
		*(pFeaturePara_64) = (uintptr_t) pValue0;
		*(pFeaturePara_64 + 1) = (uintptr_t) pValue1;

		ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

		*(pFeaturePara_64) = *pValue0;
		*(pFeaturePara_64 + 1) = *pValue1;
		kfree(pValue0);
		kfree(pValue1);
	}
	break;

	case SENSOR_FEATURE_GET_EV_AWB_REF:
		{
			struct SENSOR_AE_AWB_REF_STRUCT *pAeAwbRef = NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;
			usr_ptr =
				(void *)(uintptr_t) (*(pFeaturePara_64));

			if (FeatureParaLen < 1 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}

			pAeAwbRef = kmalloc(
					sizeof(struct SENSOR_AE_AWB_REF_STRUCT),
					GFP_KERNEL);
			if (pAeAwbRef == NULL) {
				kfree(pFeaturePara);
				PK_PR_ERR(" ioctl allocate mem failed\n");
				return -ENOMEM;
			}
			memset(pAeAwbRef,
				0x0,
				sizeof(struct SENSOR_AE_AWB_REF_STRUCT));
			*(pFeaturePara_64) = (uintptr_t) pAeAwbRef;

			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

			if (copy_to_user((void __user *)usr_ptr,
				(void *)pAeAwbRef,
				sizeof(struct SENSOR_AE_AWB_REF_STRUCT))) {

				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pAeAwbRef);
			*(pFeaturePara_64) = (uintptr_t) usr_ptr;
		}
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		{
			struct SENSOR_WINSIZE_INFO_STRUCT *pCrop = NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;
			usr_ptr =
				(void *)(uintptr_t) (*(pFeaturePara_64 + 1));

			if (FeatureParaLen < 2 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}

			pCrop = kmalloc(
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT),
				GFP_KERNEL);
			if (pCrop == NULL) {
				kfree(pFeaturePara);
				PK_PR_ERR(" ioctl allocate mem failed\n");
				return -ENOMEM;
			}
			memset(pCrop,
				0x0,
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			*(pFeaturePara_64 + 1) = (uintptr_t) pCrop;

			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

			if (copy_to_user((void __user *)usr_ptr,
				(void *)pCrop,
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT))) {

				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pCrop);
			*(pFeaturePara_64 + 1) = (uintptr_t) usr_ptr;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO2:
		{
			struct SENSOR_VC_INFO2_STRUCT *pVcInfo2 = NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;

			usr_ptr =
				(void *)(uintptr_t) (*(pFeaturePara_64 + 1));

			if (FeatureParaLen < 2 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}

			pVcInfo2 =
				kmalloc(sizeof(struct SENSOR_VC_INFO2_STRUCT),
				GFP_KERNEL);

			if (pVcInfo2 == NULL) {
				pr_err("ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}
			memset(pVcInfo2,
				0x0,
				sizeof(struct SENSOR_VC_INFO2_STRUCT));

			*(pFeaturePara_64 + 1) = (uintptr_t) pVcInfo2;

			ret =
			imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

			if (copy_to_user
			    ((void __user *)usr_ptr, (void *)pVcInfo2,
			     sizeof(struct SENSOR_VC_INFO2_STRUCT))) {

				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pVcInfo2);
			*(pFeaturePara_64 + 1) = (uintptr_t) usr_ptr;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		{
			struct SENSOR_VC_INFO_STRUCT *pVcInfo = NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;
			usr_ptr =
				(void *)(uintptr_t) (*(pFeaturePara_64 + 1));

			if (FeatureParaLen < 2 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}

			pVcInfo = kmalloc(sizeof(struct SENSOR_VC_INFO_STRUCT),
					GFP_KERNEL);
			if (pVcInfo == NULL) {
				PK_PR_ERR(" ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}
			memset(pVcInfo,
				0x0,
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			*(pFeaturePara_64 + 1) = (uintptr_t) pVcInfo;

			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

			if (copy_to_user
			    ((void __user *)usr_ptr, (void *)pVcInfo,
			     sizeof(struct SENSOR_VC_INFO_STRUCT))) {

				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pVcInfo);
			*(pFeaturePara_64 + 1) = (uintptr_t) usr_ptr;
		}
		break;

	case SENSOR_FEATURE_GET_PDAF_INFO:
		{
			struct SET_PD_BLOCK_INFO_T *pPdInfo = NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;
			usr_ptr =
				(void *)(uintptr_t) (*(pFeaturePara_64 + 1));

			if (FeatureParaLen < 2 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}

			pPdInfo = kmalloc(sizeof(struct SET_PD_BLOCK_INFO_T),
					GFP_KERNEL);
			if (pPdInfo == NULL) {
				kfree(pFeaturePara);
				PK_PR_ERR(" ioctl allocate mem failed\n");
				return -ENOMEM;
			}
			memset(pPdInfo, 0x0,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			*(pFeaturePara_64 + 1) = (uintptr_t) pPdInfo;

			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

			if (copy_to_user((void __user *)usr_ptr,
					(void *)pPdInfo,
					sizeof(struct SET_PD_BLOCK_INFO_T))) {

				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pPdInfo);
			*(pFeaturePara_64 + 1) = (uintptr_t) usr_ptr;
		}
		break;

	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		{
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;
			kal_uint32 u4RegLen = (*pFeaturePara_64);
			void *usr_ptr_Reg =
				(void *)(uintptr_t) (*(pFeaturePara_64 + 1));
			kal_uint32 *pReg = NULL;

			if (FeatureParaLen < 2 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}

			/* buffer size exam */
			if ((sizeof(kal_uint8) * u4RegLen) >
					IMGSENSOR_FEATURE_PARA_LEN_MAX) {
				kfree(pFeaturePara);
				PK_PR_ERR(" buffer size (%u) is too large\n",
					u4RegLen);
				return -EINVAL;
			}
			pReg = kmalloc_array(u4RegLen,
					sizeof(kal_uint8),
					GFP_KERNEL);
			if (pReg == NULL) {
				kfree(pFeaturePara);
				PK_PR_ERR(" ioctl allocate mem failed\n");
				return -ENOMEM;
			}

			memset(pReg, 0x0, sizeof(kal_uint8) * u4RegLen);

			if (copy_from_user((void *)pReg,
					   (void *)usr_ptr_Reg,
					   sizeof(kal_uint8) * u4RegLen)) {

				PK_PR_ERR(
				"[CAMERA_HW]ERROR: copy from user fail\n");
			}

			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pReg,
					(unsigned int *)&u4RegLen);

			if (copy_to_user((void __user *)usr_ptr_Reg,
					 (void *)pReg,
					 sizeof(kal_uint8) * u4RegLen)) {

				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pReg);
		}

		break;

	case SENSOR_FEATURE_SET_AF_WINDOW:
	case SENSOR_FEATURE_SET_AE_WINDOW:
		{
			MUINT32 *pApWindows = NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;
			usr_ptr =
				(void *)(uintptr_t) (*(pFeaturePara_64));

			if (FeatureParaLen < 1 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}

			pApWindows = kmalloc(sizeof(MUINT32) * 6, GFP_KERNEL);
			if (pApWindows == NULL) {
				kfree(pFeaturePara);
				PK_PR_ERR(" ioctl allocate mem failed\n");
				return -ENOMEM;
			}
			memset(pApWindows, 0x0, sizeof(MUINT32) * 6);
			*(pFeaturePara_64) = (uintptr_t) pApWindows;

			if (copy_from_user((void *)pApWindows,
					   (void *)usr_ptr,
					   sizeof(MUINT32) * 6)) {

				PK_PR_ERR(
				"[CAMERA_HW]ERROR: copy from user fail\n");
			}

			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);
			kfree(pApWindows);
			*(pFeaturePara_64) = (uintptr_t) usr_ptr;
		}
		break;

	case SENSOR_FEATURE_GET_EXIF_INFO:
		{
			struct SENSOR_EXIF_INFO_STRUCT *pExif = NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;
			usr_ptr =
				(void *)(uintptr_t) (*(pFeaturePara_64));

			if (FeatureParaLen < 1 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}

			pExif = kmalloc(sizeof(struct SENSOR_EXIF_INFO_STRUCT),
					GFP_KERNEL);
			if (pExif == NULL) {
				kfree(pFeaturePara);
				PK_PR_ERR(" ioctl allocate mem failed\n");
				return -ENOMEM;
			}
			memset(pExif, 0x0,
				sizeof(struct SENSOR_EXIF_INFO_STRUCT));
			*(pFeaturePara_64) = (uintptr_t) pExif;

			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

			if (copy_to_user((void __user *)usr_ptr,
				(void *)pExif,
				sizeof(struct SENSOR_EXIF_INFO_STRUCT))) {

				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pExif);
			*(pFeaturePara_64) = (uintptr_t) usr_ptr;
		}
		break;

	case SENSOR_FEATURE_GET_SHUTTER_GAIN_AWB_GAIN:
		{

			struct SENSOR_AE_AWB_CUR_STRUCT *pCurAEAWB = NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;
			usr_ptr =
				(void *)(uintptr_t) (*(pFeaturePara_64));

			if (FeatureParaLen < 1 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}

			pCurAEAWB = kmalloc(
					sizeof(struct SENSOR_AE_AWB_CUR_STRUCT),
					GFP_KERNEL);
			if (pCurAEAWB == NULL) {
				kfree(pFeaturePara);
				PK_PR_ERR(" ioctl allocate mem failed\n");
				return -ENOMEM;
			}
			memset(pCurAEAWB, 0x0,
				sizeof(struct SENSOR_AE_AWB_CUR_STRUCT));
			*(pFeaturePara_64) = (uintptr_t) pCurAEAWB;

			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

			if (copy_to_user((void __user *)usr_ptr,
				(void *)pCurAEAWB,
				sizeof(struct SENSOR_AE_AWB_CUR_STRUCT))) {

				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pCurAEAWB);
			*(pFeaturePara_64) = (uintptr_t) usr_ptr;
		}
		break;

	case SENSOR_FEATURE_GET_DELAY_INFO:
		{
			struct SENSOR_DELAY_INFO_STRUCT *pDelayInfo = NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;
			usr_ptr =
				(void *)(uintptr_t) (*(pFeaturePara_64));

			if (FeatureParaLen < 1 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}

			pDelayInfo = kmalloc(
				sizeof(struct SENSOR_DELAY_INFO_STRUCT),
				GFP_KERNEL);

			if (pDelayInfo == NULL) {
				kfree(pFeaturePara);
				PK_PR_ERR(" ioctl allocate mem failed\n");
				return -ENOMEM;
			}
			memset(pDelayInfo, 0x0,
				sizeof(struct SENSOR_DELAY_INFO_STRUCT));
			*(pFeaturePara_64) = (uintptr_t) pDelayInfo;

			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

			if (copy_to_user((void __user *)usr_ptr,
				(void *)pDelayInfo,
				sizeof(struct SENSOR_DELAY_INFO_STRUCT))) {

				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pDelayInfo);
			*(pFeaturePara_64) = (uintptr_t) usr_ptr;

		}
		break;

	case SENSOR_FEATURE_GET_AE_FLASHLIGHT_INFO:
		{
			struct SENSOR_FLASHLIGHT_AE_INFO_STRUCT *pFlashInfo =
				NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;
			usr_ptr =
				(void *)(uintptr_t) (*(pFeaturePara_64));

			if (FeatureParaLen < 1 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}

			pFlashInfo = kmalloc(
				sizeof(struct SENSOR_FLASHLIGHT_AE_INFO_STRUCT),
				GFP_KERNEL);

			if (pFlashInfo == NULL) {
				kfree(pFeaturePara);
				PK_PR_ERR(" ioctl allocate mem failed\n");
				return -ENOMEM;
			}
			memset(pFlashInfo, 0x0,
			sizeof(struct SENSOR_FLASHLIGHT_AE_INFO_STRUCT));
			*(pFeaturePara_64) = (uintptr_t) pFlashInfo;

			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

			if (copy_to_user((void __user *)usr_ptr,
			(void *)pFlashInfo,
			sizeof(struct SENSOR_FLASHLIGHT_AE_INFO_STRUCT))) {

				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pFlashInfo);
			*(pFeaturePara_64) = (uintptr_t) usr_ptr;

		}
		break;
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		{
#define GAIN_TBL_SIZE 4096

			char *pGain_tbl = NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;

			kal_uint32 buf_sz =
					(kal_uint32) (*(pFeaturePara_64));
			void *usr_ptr =
				(void *)(uintptr_t) (*(pFeaturePara_64 + 1));

			if (FeatureParaLen < 2 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}

			if (buf_sz == 0 || usr_ptr == NULL) {
				ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);
			} else {
				if (buf_sz > GAIN_TBL_SIZE) {
					kfree(pFeaturePara);
					PK_PR_ERR(
					"gain tbl size (%u) can't larger than %d bytes\n",
					buf_sz, GAIN_TBL_SIZE);
					return -EINVAL;
				}
				pGain_tbl = kmalloc(
				sizeof(char) * buf_sz, GFP_KERNEL);
				if (pGain_tbl == NULL) {
					kfree(pFeaturePara);
					PK_PR_ERR(
						"ioctl allocate mem failed\n");
					return -ENOMEM;
				}
				memset(pGain_tbl, 0x0, sizeof(char) * buf_sz);
				if (pFeaturePara_64 != NULL) {
					*(pFeaturePara_64 + 1)
						= (uintptr_t) pGain_tbl;
				}
				ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

				if (copy_to_user((void __user *)usr_ptr,
						 (void *)pGain_tbl, buf_sz)) {
					PK_DBG("copy_to_user fail\n");
				}
				kfree(pGain_tbl);
				*(pFeaturePara_64 + 1) = (uintptr_t) usr_ptr;
			}
		}
		break;
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
		{
#define _DATA_SIZE 64
			char *p_data = NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;
			void *usr_ptr =
				(void *)(uintptr_t) (*(pFeaturePara_64 + 1));
			if (FeatureParaLen < 3 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}
			p_data = kmalloc(
				sizeof(char) * _DATA_SIZE, GFP_KERNEL);
			if (p_data == NULL) {
				kfree(pFeaturePara);
				PK_DBG(" ioctl allocate mem failed\n");
				return -ENOMEM;
			}
			if (copy_from_user((void *)p_data,
				(void __user *)usr_ptr, _DATA_SIZE)) {
				kfree(pFeaturePara);
				kfree(p_data);
				PK_DBG("[CAMERA_HW]ERROR: copy_from_user fail\n");
				return -ENOMEM;
			}

			if (pFeaturePara_64 != NULL)
				*(pFeaturePara_64 + 1) = (uintptr_t) p_data;

			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

			if (copy_to_user((void __user *)usr_ptr,
					 (void *)p_data, _DATA_SIZE)) {
				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(p_data);
			*(pFeaturePara_64 + 1) = (uintptr_t) usr_ptr;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
	case SENSOR_FEATURE_GET_4CELL_DATA:
		{
#define PDAF_DATA_SIZE 4096
			char *pPdaf_data = NULL;
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;
			usr_ptr =
				(void *)(uintptr_t) (*(pFeaturePara_64 + 1));
			buf_sz =
				(kal_uint32) (*(pFeaturePara_64 + 2));

			if (FeatureParaLen < 3 * sizeof(unsigned long long)) {
				PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
				kfree(pFeaturePara);
				return -EINVAL;
			}
			/* buffer size exam */
			if (buf_sz > PDAF_DATA_SIZE) {
				kfree(pFeaturePara);
				PK_PR_ERR(
				"buffer size (%u) can't larger than %d bytes\n",
					  buf_sz, PDAF_DATA_SIZE);
				return -EINVAL;
			}

			pPdaf_data = kmalloc(
				sizeof(char) * PDAF_DATA_SIZE, GFP_KERNEL);
			if (pPdaf_data == NULL) {
				kfree(pFeaturePara);
				PK_PR_ERR(" ioctl allocate mem failed\n");
				return -ENOMEM;
			}
			memset(pPdaf_data, 0xff, sizeof(char) * PDAF_DATA_SIZE);

			if (pFeaturePara_64 != NULL)
				*(pFeaturePara_64 + 1) = (uintptr_t) pPdaf_data;


			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

			if (copy_to_user((void __user *)usr_ptr,
					 (void *)pPdaf_data, buf_sz)) {
				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pPdaf_data);
			*(pFeaturePara_64 + 1) = (uintptr_t) usr_ptr;
		}
		break;
	case SENSOR_FEATURE_SET_LSC_TBL:
		{
#define LSC_TBL_DATA_SIZE 1024
			unsigned long long *pFeaturePara_64 =
				(unsigned long long *)pFeaturePara;
			kal_uint32 u4RegLen =
				(kal_uint32)(*pFeaturePara_64);
			void *usr_ptr_Reg =
				(void *)(uintptr_t) (*(pFeaturePara_64 + 1));
			kal_uint32 index = *(pFeaturePara_64 + 2);
			kal_uint8 *pReg = NULL;

			/* buffer size exam */
			if ((sizeof(kal_uint8) * u4RegLen) >
			    IMGSENSOR_FEATURE_PARA_LEN_MAX ||
			    (u4RegLen > LSC_TBL_DATA_SIZE || u4RegLen < 0)) {
				kfree(pFeaturePara);
				PK_PR_ERR(" buffer size (%u) is too large\n",
					u4RegLen);
				return -EINVAL;
			}
			pReg = kmalloc_array((u4RegLen + 1), sizeof(kal_uint8),
					GFP_KERNEL);
			if (pReg == NULL) {
				kfree(pFeaturePara);
				PK_PR_ERR(" ioctl allocate mem failed\n");
				return -ENOMEM;
			}

			memset(pReg, 0x0, sizeof(kal_uint8) * (u4RegLen + 1));

			if (copy_from_user((void *)pReg,
					   (void *)usr_ptr_Reg,
					   sizeof(kal_uint8) * u4RegLen)) {
				PK_PR_ERR(
				"[CAMERA_HW]ERROR: copy from user fail\n");
			}
			*(((kal_uint8 *)pReg) + u4RegLen) = index;

			ret = imgsensor_sensor_feature_control(psensor,
						pFeatureCtrl->FeatureId,
						(unsigned char *)pReg,
						(unsigned int *)&u4RegLen);

			kfree(pReg);
		}

		break;
	case SENSOR_FEATURE_DEBUG_IMGSENSOR:
		imgsensor_hw_dump(&gimgsensor.hw);
		break;
#ifdef SENINF_N3D_SUPPORT
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);
		set_sensor_streaming_state((int)psensor->inst.sensor_idx, 0);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);
		set_sensor_streaming_state((int)psensor->inst.sensor_idx, 1);
		break;
#endif
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		{
			pr_debug("SENSOR_FEATURE_SET_TEST_PATTERN");
			pFeaturePara_64 = (unsigned long long *)pFeaturePara;
			usr_ptr =
				(void *)(uintptr_t)(*(pFeaturePara_64 + 1));
			if (usr_ptr) {
				if (FeatureParaLen < 2 * sizeof(unsigned long long)) {
					PK_DBG("FeatureParaLen is too small %d\n", FeatureParaLen);
					kfree(pFeaturePara);
					return -EINVAL;
				}
				pData = kmalloc(sizeof(struct SET_SENSOR_PATTERN_SOLID_COLOR),
					GFP_KERNEL);

				if (pData == NULL) {
					kfree(pFeaturePara);
					PK_DBG("No color data %d\n");
					return -ENOMEM;
				}
				memset(pData, 0x0,
					sizeof(struct SET_SENSOR_PATTERN_SOLID_COLOR));

				if (copy_from_user
					((void *)pData, (void __user *)usr_ptr,
					sizeof(struct SET_SENSOR_PATTERN_SOLID_COLOR))) {
					kfree(pData);
					PK_DBG("[CAMERA_HW]ERROR: copy_from_user fail\n");
				}
				//pr_debug("%x %x %x %x",pData->COLOR_R,pData->COLOR_Gr,
				//pData->COLOR_Gb,pData->COLOR_B);
				memcpy((void *)(pFeaturePara_64 + 1), (void *)pData,
					sizeof(struct SET_SENSOR_PATTERN_SOLID_COLOR));
			}
			ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);

			kfree(pData);
		}
		break;
	default:
		ret = imgsensor_sensor_feature_control(psensor,
					pFeatureCtrl->FeatureId,
					(unsigned char *)pFeaturePara,
					(unsigned int *)&FeatureParaLen);
#ifdef CONFIG_MTK_CCU
		if (pFeatureCtrl->FeatureId == SENSOR_FEATURE_SET_FRAMERATE)
			ccu_set_current_fps((int32_t)psensor->inst.sensor_idx,
			*((int32_t *) pFeaturePara));
#endif
		break;
	}

	if (FeatureParaLen != 0 &&
	    pFeaturePara != NULL &&
	    pFeatureCtrl->pFeaturePara != NULL &&
	    copy_to_user((void __user *)pFeatureCtrl->pFeaturePara,
			(void *)pFeaturePara,
			 FeatureParaLen)) {
		kfree(pFeaturePara);
		PK_DBG(
		"[CAMERA_HW][pSensorRegData] ioctl copy to user failed\n");
		return -EFAULT;
	}

	kfree(pFeaturePara);

	if (pFeatureCtrl->pFeatureParaLen != NULL &&
			copy_to_user(
			(void __user *)pFeatureCtrl->pFeatureParaLen,
			(void *)&FeatureParaLen, sizeof(unsigned int))) {
		PK_DBG(
		"[CAMERA_HW][pFeatureParaLen] ioctl copy to user failed\n");
		return -EFAULT;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static int compat_get_imagesensor_getinfo_struct(
		struct COMPAT_IMAGESENSOR_GETINFO_STRUCT __user *data32,
		struct IMAGESENSOR_GETINFO_STRUCT __user *data)
{
	compat_uptr_t p;
	compat_uint_t i;
	int err;

	err = get_user(i, &data32->SensorId);
	err |= put_user(i, &data->SensorId);
	err |= get_user(p, &data32->pInfo);
	err |= put_user(compat_ptr(p), &data->pInfo);
	err |= get_user(p, &data32->pSensorResolution);
	err |= put_user(compat_ptr(p), &data->pSensorResolution);
	return err;
}

static int compat_put_imagesensor_getinfo_struct(
		struct COMPAT_IMAGESENSOR_GETINFO_STRUCT __user *data32,
		struct IMAGESENSOR_GETINFO_STRUCT __user *data)
{
	/* compat_uptr_t p; */
	compat_uint_t i;
	int err;

	err = get_user(i, &data->SensorId);
	err |= put_user(i, &data32->SensorId);

	return err;
}

static int compat_get_acdk_sensor_featurecontrol_struct(
		struct COMPAT_ACDK_SENSOR_FEATURECONTROL_STRUCT
		__user *data32,
		struct ACDK_SENSOR_FEATURECONTROL_STRUCT __user *
		data)
{
	compat_uptr_t p;
	compat_uint_t i;
	int err;

	err = get_user(i, &data32->InvokeCamera);
	err |= put_user(i, &data->InvokeCamera);
	err |= get_user(i, &data32->FeatureId);
	err |= put_user(i, &data->FeatureId);
	err |= get_user(p, &data32->pFeaturePara);
	err |= put_user(compat_ptr(p), &data->pFeaturePara);
	err |= get_user(p, &data32->pFeatureParaLen);
	err |= put_user(compat_ptr(p), &data->pFeatureParaLen);
	return err;
}

static int compat_put_acdk_sensor_featurecontrol_struct(
		struct COMPAT_ACDK_SENSOR_FEATURECONTROL_STRUCT
		__user *data32,
		struct ACDK_SENSOR_FEATURECONTROL_STRUCT __user *
		data)
{
	MUINT8 *p;
	MUINT32 *q;
	compat_uint_t i;
	int err;

	err = get_user(i, &data->InvokeCamera);
	err |= put_user(i, &data32->InvokeCamera);
	err |= get_user(i, &data->FeatureId);
	err |= put_user(i, &data32->FeatureId);
	/* Assume pointer is not change */

	err |= get_user(p, &data->pFeaturePara);
	err |= put_user(ptr_to_compat(p), &data32->pFeaturePara);
	err |= get_user(q, &data->pFeatureParaLen);
	err |= put_user(ptr_to_compat(q), &data32->pFeatureParaLen);

	return err;
}

static int compat_get_acdk_sensor_control_struct(
		struct COMPAT_ACDK_SENSOR_CONTROL_STRUCT __user *data32,
		struct ACDK_SENSOR_CONTROL_STRUCT __user *data)
{
	compat_uptr_t p;
	compat_uint_t i;
	int err;

	err = get_user(i, &data32->InvokeCamera);
	err |= put_user(i, &data->InvokeCamera);
	err |= get_user(i, &data32->ScenarioId);
	err |= put_user(i, &data->ScenarioId);
	err |= get_user(p, &data32->pImageWindow);
	err |= put_user(compat_ptr(p), &data->pImageWindow);
	err |= get_user(p, &data32->pSensorConfigData);
	err |= put_user(compat_ptr(p), &data->pSensorConfigData);
	return err;
}

static int compat_put_acdk_sensor_control_struct(
		struct COMPAT_ACDK_SENSOR_CONTROL_STRUCT __user *data32,
		struct ACDK_SENSOR_CONTROL_STRUCT __user *data)
{
	/* compat_uptr_t p; */
	compat_uint_t i;
	int err;

	err = get_user(i, &data->InvokeCamera);
	err |= put_user(i, &data32->InvokeCamera);
	err |= get_user(i, &data->ScenarioId);
	err |= put_user(i, &data32->ScenarioId);

	return err;
}

static long imgsensor_compat_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	long ret;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_KDIMGSENSORIOC_X_FEATURECONCTROL:
	{
		struct COMPAT_ACDK_SENSOR_FEATURECONTROL_STRUCT __user
		*data32;
		struct ACDK_SENSOR_FEATURECONTROL_STRUCT __user *data;
		int err;

		/* PK_DBG(
		 *"[CAMERA SENSOR] CAOMPAT_KDIMGSENSORIOC_X_FEATURECONCTROL\n");
		 */

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err =
		compat_get_acdk_sensor_featurecontrol_struct(data32, data);
		if (err)
			return err;

		ret =
		    filp->f_op->unlocked_ioctl(filp,
					KDIMGSENSORIOC_X_FEATURECONCTROL,
					(unsigned long)data);
		err =
		compat_put_acdk_sensor_featurecontrol_struct(data32, data);

		if (err != 0)
			PK_PR_ERR(
		"[CAMERA SENSOR] compat_put_acdk_sensor_featurecontrol_struct failed\n");
		return ret;
	}
	case COMPAT_KDIMGSENSORIOC_X_CONTROL:
		{
			struct COMPAT_ACDK_SENSOR_CONTROL_STRUCT __user *data32;
			struct ACDK_SENSOR_CONTROL_STRUCT __user *data;
			int err;

			PK_DBG(
			"[CAMERA SENSOR] CAOMPAT_KDIMGSENSORIOC_X_CONTROL\n");

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err =
			compat_get_acdk_sensor_control_struct(data32, data);
			if (err)
				return err;
			ret =
			    filp->f_op->unlocked_ioctl(filp,
						KDIMGSENSORIOC_X_CONTROL,
						(unsigned long)data);
			err =
			  compat_put_acdk_sensor_control_struct(data32, data);

			if (err != 0)
				PK_PR_ERR(
			"[CAMERA SENSOR] compat_put_acdk_sensor_control_struct failed\n");
			return ret;
		}
	case COMPAT_KDIMGSENSORIOC_X_GETINFO2:
		{
			struct COMPAT_IMAGESENSOR_GETINFO_STRUCT __user *data32;
			struct IMAGESENSOR_GETINFO_STRUCT __user *data;
			int err;

			PK_DBG(
			"[CAMERA SENSOR] CAOMPAT_KDIMGSENSORIOC_X_GETINFO2\n");

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err =
			  compat_get_imagesensor_getinfo_struct(data32, data);
			if (err)
				return err;
			ret =
			    filp->f_op->unlocked_ioctl(filp,
					KDIMGSENSORIOC_X_GETINFO2,
					(unsigned long)data);
			err =
			  compat_put_imagesensor_getinfo_struct(data32, data);

			if (err != 0)
				PK_PR_ERR(
			"[CAMERA SENSOR] compat_put_acdk_sensor_getinfo_struct failed\n");
			return ret;
		}

	default:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	}
}
#endif

/******************************************************************************
 * imgsensor_ioctl
 ******************************************************************************/
static long imgsensor_ioctl(
		struct file *a_pstFile,
		unsigned int a_u4Command, unsigned long a_u4Param)
{
	int i4RetValue = 0;
	void *pBuff = NULL;

	if (_IOC_DIR(a_u4Command) != _IOC_NONE) {
		pBuff = kmalloc(_IOC_SIZE(a_u4Command), GFP_KERNEL);
		if (pBuff == NULL) {
			PK_DBG("[CAMERA SENSOR] ioctl allocate mem failed\n");
			i4RetValue = -ENOMEM;
			goto CAMERA_HW_Ioctl_EXIT;
		}

		if (_IOC_WRITE & _IOC_DIR(a_u4Command)) {
			if (copy_from_user(pBuff, (void *)a_u4Param,
			_IOC_SIZE(a_u4Command))) {
				PK_DBG(
			"[CAMERA SENSOR] ioctl copy from user failed\n");
				i4RetValue = -EFAULT;
				goto CAMERA_HW_Ioctl_EXIT;
			}
		}
	} else {
		i4RetValue = -EFAULT;
		goto CAMERA_HW_Ioctl_EXIT;
	}

	switch (a_u4Command) {
	case KDIMGSENSORIOC_X_GETINFO2:
		i4RetValue = adopt_CAMERA_HW_GetInfo2(pBuff);
		break;

	case KDIMGSENSORIOC_X_FEATURECONCTROL:
		i4RetValue = adopt_CAMERA_HW_FeatureControl(pBuff);
		break;

	case KDIMGSENSORIOC_X_CONTROL:
		i4RetValue = adopt_CAMERA_HW_Control(pBuff);
		break;

	default:
		PK_DBG("No such command %d\n", a_u4Command);
		i4RetValue = -EPERM;
		goto CAMERA_HW_Ioctl_EXIT;
		break;
	}

	if ((_IOC_READ & _IOC_DIR(a_u4Command)) &&
	    copy_to_user((void __user *)a_u4Param, pBuff,
			_IOC_SIZE(a_u4Command))) {
		PK_DBG("[CAMERA SENSOR] ioctl copy to user failed\n");
		i4RetValue = -EFAULT;
		goto CAMERA_HW_Ioctl_EXIT;
	}

CAMERA_HW_Ioctl_EXIT:
	if (pBuff != NULL) {
		kfree(pBuff);
		pBuff = NULL;
	}

	return i4RetValue;
}

static int imgsensor_open(struct inode *a_pstInode, struct file *a_pstFile)
{
	struct IMGSENSOR *pimgsensor = &gimgsensor;

	mutex_lock(&gimgsensor_open_mutex);

	atomic_inc(&pimgsensor->imgsensor_open_cnt);
	PK_DBG("%s %d\n", __func__,
		atomic_read(&pimgsensor->imgsensor_open_cnt));

	mutex_unlock(&gimgsensor_open_mutex);
	return 0;
}

#if defined(CONFIG_MTK_CAM_SECURE_I2C)
static void imgsensor_release_secure_flag(void)
{
	struct IMGSENSOR *pimgsensor = &gimgsensor;

	pimgsensor->imgsensor_sec_flag = 0;
	PK_DBG("release secure flag! %d", (int)pimgsensor->imgsensor_sec_flag);
}
#endif

static int imgsensor_release(struct inode *a_pstInode, struct file *a_pstFile)
{
	struct IMGSENSOR *pimgsensor = &gimgsensor;

	mutex_lock(&gimgsensor_open_mutex);

	atomic_dec(&pimgsensor->imgsensor_open_cnt);
	if (atomic_read(&pimgsensor->imgsensor_open_cnt) == 0) {
		imgsensor_hw_release_all(&pimgsensor->hw);

#ifdef CONFIG_MTK_CAM_SECURE_I2C
		imgsensor_release_secure_flag();/* to reset sensor status */
		imgsensor_ca_release();
#endif

	}

	PK_DBG("%s %d\n", __func__,
		atomic_read(&pimgsensor->imgsensor_open_cnt));

	mutex_unlock(&gimgsensor_open_mutex);
	return 0;
}

static const struct file_operations gimgsensor_file_operations = {
	.owner          = THIS_MODULE,
	.open           = imgsensor_open,
	.release        = imgsensor_release,
	.unlocked_ioctl = imgsensor_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = imgsensor_compat_ioctl
#endif
};

static int imgsensor_probe(struct platform_device *pplatform_device)
{
	struct IMGSENSOR *pimgsensor = &gimgsensor;
	struct IMGSENSOR_HW *phw = &pimgsensor->hw;
	struct device *pdevice;

	/* Register char driver */
	if (alloc_chrdev_region(&pimgsensor->dev_no, 0, 1,
			IMGSENSOR_DEV_NAME)) {
		PK_DBG("[CAMERA SENSOR] Allocate device no failed\n");
		return -EAGAIN;
	}

	/* Allocate driver */
	pimgsensor->pcdev = cdev_alloc();
	if (pimgsensor->pcdev == NULL) {
		unregister_chrdev_region(pimgsensor->dev_no, 1);
		PK_DBG("[CAMERA SENSOR] Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	/* Attatch file operation. */
	cdev_init(pimgsensor->pcdev, &gimgsensor_file_operations);
	pimgsensor->pcdev->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(pimgsensor->pcdev, pimgsensor->dev_no, 1)) {
		PK_DBG("Attatch file operation failed\n");
		unregister_chrdev_region(pimgsensor->dev_no, 1);
		return -EAGAIN;
	}

	pimgsensor->pclass = class_create(THIS_MODULE, "sensordrv");
	if (IS_ERR(pimgsensor->pclass)) {
		int ret = PTR_ERR(pimgsensor->pclass);

		PK_DBG("Unable to create class, err = %d\n", ret);
		return ret;
	}

	pdevice = device_create(pimgsensor->pclass, NULL,
			pimgsensor->dev_no, NULL, IMGSENSOR_DEV_NAME);
	pdevice->of_node =
		of_find_compatible_node(NULL, NULL, "mediatek,imgsensor");
	if (!pdevice->of_node) {
		PK_PR_ERR("Get cust camera node failed!\n");
		return -ENODEV;
	}

	phw->common.pplatform_device = pplatform_device;

	imgsensor_hw_init(phw);
	imgsensor_i2c_create();
	imgsensor_proc_init();
	imgsensor_init_sensor_list();

#ifdef IMGSENSOR_OC_ENABLE
	imgsensor_oc_init();
#endif

	return 0;
}

static int imgsensor_remove(struct platform_device *pplatform_device)
{
	struct IMGSENSOR *pimgsensor = &gimgsensor;

	imgsensor_i2c_delete();

	/* Release char driver */
	cdev_del(pimgsensor->pcdev);
	unregister_chrdev_region(pimgsensor->dev_no, 1);

	device_destroy(pimgsensor->pclass, pimgsensor->dev_no);
	class_destroy(pimgsensor->pclass);

	return 0;
}

static int imgsensor_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int imgsensor_resume(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id gimgsensor_of_device_id[] = {
	{.compatible = "mediatek,imgsensor",},
	{}
};
#endif

static struct platform_driver gimgsensor_platform_driver = {
	.probe   = imgsensor_probe,
	.remove  = imgsensor_remove,
	.suspend = imgsensor_suspend,
	.resume  = imgsensor_resume,
	.driver  = {
		   .name  = "image_sensor",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = gimgsensor_of_device_id,
#endif
		}
};

static int __init imgsensor_init(void)
{
	PK_DBG("[camerahw_probe] start\n");

	if (platform_driver_register(&gimgsensor_platform_driver)) {
		PK_PR_ERR("failed to register CAMERA_HW driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit imgsensor_exit(void)
{
	platform_driver_unregister(&gimgsensor_platform_driver);
}
#ifdef NEED_LATE_INITCALL
	late_initcall(imgsensor_init);
#else
	module_init(imgsensor_init);
#endif
module_exit(imgsensor_exit);

MODULE_DESCRIPTION("image sensor driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL v2");

