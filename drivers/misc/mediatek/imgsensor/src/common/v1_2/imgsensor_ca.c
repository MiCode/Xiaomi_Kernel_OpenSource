/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "imgsensor_ca.h"
#include "tee_client_api.h"

#include "tee_client_api_cust.h"
#define TEEC_Result UINT32

#define PREFIX "[imgsensor_ca]"

#define cam_pr_debug(fmt, arg...)  pr_debug(PREFIX fmt, ##arg)
#define cam_pr_info(fmt, arg...)   pr_debug(PREFIX fmt, ##arg)
/* #define _TRACE(fmt, arg...)     pr_debug(fmt, ##arg) */
#define _TRACE(fmt, arg...)


#define IMGSENSOR_UUID { 0x7b665120, 0x2121, 0x4487, \
	{ 0xba, 0x71, 0x0a, 0x51, 0xd7, 0xea, 0x78, 0xfe } }

const struct TEEC_UUID TEEC_IMGSENSOR_UUID = IMGSENSOR_UUID;
static struct TEEC_Context imgsensor_tci_context;
static struct TEEC_Session imgsensor_tci_session;


/*#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)*/
/*const char* MY_UUID = "bta_loader";*/
/*#endif*/

/*
 * #if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
 * const char* MY_UUID = NULL;
 * #endif
 */

static int secure_i2c_bus = 3; /*initiate at _tee_cmd_get_i2c_bus*/

static atomic_t ca_open_cnt;

unsigned int imgsensor_ca_open(void)
{
	TEEC_Result ret = TEEC_SUCCESS;
	struct TEEC_Operation teeOperation;
	struct command_params c_params = {0};

	_TRACE("%s\n", __func__);
	if (atomic_inc_return(&ca_open_cnt) == 1) {
		memset(&teeOperation, 0, sizeof(struct TEEC_Operation));
		memset(&imgsensor_tci_context, 0,
				sizeof(imgsensor_tci_context));
		memset(&imgsensor_tci_session, 0,
				sizeof(imgsensor_tci_session));

		ret = TEEC_InitializeContext(NULL, &imgsensor_tci_context);
		if (ret != TEEC_SUCCESS) {
			cam_pr_info("teec_initialize_context failed: %x\n",
				ret);
		} else {
			ret = TEEC_OpenSession(
				&imgsensor_tci_context, &imgsensor_tci_session,
				&TEEC_IMGSENSOR_UUID, TEEC_LOGIN_PUBLIC, NULL,
				&teeOperation, NULL);
			if (ret != TEEC_SUCCESS) {
				TEEC_FinalizeContext(&imgsensor_tci_context);
				memset(&imgsensor_tci_context, 0,
					sizeof(imgsensor_tci_context));
				atomic_set(&ca_open_cnt, 0);
				cam_pr_info("teec_open_session failed: %x\n",
					ret);
				/*TEEC_FinalizeContext(&m4u_tci_context);*/
			} else {
				if (imgsensor_ca_invoke_command(
					IMGSENSOR_TEE_CMD_GET_I2C_BUS,
					c_params, &ret) != 0) {
					cam_pr_info("Error: SECURE_I2C_BUS is %d",
						secure_i2c_bus);
				} else {
					cam_pr_debug("SECURE_I2C_BUS is %d",
						secure_i2c_bus);
					i2c_tui_enable_clock(secure_i2c_bus);
				}
			}
		}
	}
	cam_pr_info("%s ca_open_cnt %d ret= %d\n",
		__func__, atomic_read(&ca_open_cnt), ret);
	_TRACE("%s exit\n", __func__);
	return ret;
}

void imgsensor_ca_close(void)
{
	_TRACE("%s\n", __func__);
	if (atomic_dec_and_test(&ca_open_cnt)) {
		TEEC_CloseSession(&imgsensor_tci_session);
		TEEC_FinalizeContext(&imgsensor_tci_context);
		memset(&imgsensor_tci_context, 0,
			sizeof(imgsensor_tci_context));
		memset(&imgsensor_tci_session, 0,
			sizeof(imgsensor_tci_session));
		cam_pr_debug("teec_finalize_context\n");
		i2c_tui_disable_clock(secure_i2c_bus);
	}
	cam_pr_info("%s ca_open_cnt %d\n",
		__func__, atomic_read(&ca_open_cnt));
	_TRACE("%s exit\n", __func__);
}

void imgsensor_ca_release(void)
{
	_TRACE("%s\n", __func__);
	if (atomic_read(&ca_open_cnt) > 0) {
		atomic_set(&ca_open_cnt, 0);
		TEEC_CloseSession(&imgsensor_tci_session);
		TEEC_FinalizeContext(&imgsensor_tci_context);
		memset(&imgsensor_tci_context, 0,
			sizeof(imgsensor_tci_context));
		memset(&imgsensor_tci_session, 0,
			sizeof(imgsensor_tci_session));
		cam_pr_debug("teec_finalize_context\n");
		i2c_tui_disable_clock(secure_i2c_bus);
	}
	cam_pr_debug("%s ca_open_cnt %d\n",
		__func__, atomic_read(&ca_open_cnt));
	_TRACE("%s exit\n", __func__);
}


TEEC_Result _tee_cmd_feature_control(
	struct command_params params, MUINT32 *ret)
{
	TEEC_Result ret_tee = TEEC_SUCCESS;
	MSDK_SENSOR_FEATURE_ENUM feature_id =
		(MSDK_SENSOR_FEATURE_ENUM)params.param0;
	struct TEEC_Operation teeOperation;
	UINT8 *feature_para = (UINT8 *)params.param1;
	UINT32 *feature_para_len = (UINT32 *)params.param2;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	_TRACE("%s\n", __func__);
	memset(&teeOperation, 0, sizeof(struct TEEC_Operation));

	teeOperation.params[0].value.a = 0xff;
	teeOperation.params[0].value.b = feature_id;
	teeOperation.params[1].tmpref.buffer = params.param1;
	teeOperation.params[2].tmpref.buffer = params.param2;



	cam_pr_debug("feature_id = %d %p %p %d %d\n",
		feature_id,
		teeOperation.params[1].tmpref.buffer,
		teeOperation.params[2].tmpref.buffer,
		teeOperation.params[1].value.a, teeOperation.params[2].value.a);
	/*cam_pr_debug("size of unsigned long long %d\n", */
	/*	sizeof(unsigned long long));*/

	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
				TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_OUTPUT,
				TEEC_MEMREF_TEMP_INOUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(UINT16)*2;
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
				TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_OUTPUT,
				TEEC_MEMREF_TEMP_INOUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(UINT32);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
				TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
				TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(unsigned long long);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
				TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
				TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(unsigned long long);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(unsigned long long);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_SET_DUAL_GAIN:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size =
			sizeof(unsigned long long)*2;
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
#if 0
	case SENSOR_FEATURE_SET_REGISTER:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size =
			sizeof(MSDK_SENSOR_REG_INFO_STRUCT);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_OUTPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size =
			sizeof(MSDK_SENSOR_REG_INFO_STRUCT);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
#endif
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
				TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_OUTPUT,
				TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(UINT32);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);

		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
				TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
				TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(unsigned long long);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
				TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_OUTPUT,
				TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(UINT32);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
				TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
				TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(UINT16)*2;
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
				TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
				TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size =
			sizeof(unsigned long long)*2;
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INOUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_OUTPUT);
		teeOperation.params[1].tmpref.size =
			sizeof(unsigned long long)*2;
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		teeOperation.params[3].tmpref.buffer =
			(void *)(uintptr_t) (*(feature_data + 1));
		teeOperation.params[3].tmpref.size = sizeof(MUINT32);
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INOUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_OUTPUT);
		teeOperation.params[1].tmpref.size =
			sizeof(unsigned long long)*3;
		teeOperation.params[2].tmpref.size = sizeof(UINT32);

		teeOperation.params[3].tmpref.buffer =
			(void *)(uintptr_t) (*(feature_data + 1));
		teeOperation.params[3].tmpref.size =
			(kal_uint32) (*(feature_data + 2));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(unsigned long long);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_OUTPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(UINT32);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(UINT32);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INOUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_OUTPUT);
		teeOperation.params[1].tmpref.size =
			sizeof(unsigned long long)*2;
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		teeOperation.params[3].tmpref.buffer =
			(void *)(uintptr_t) (*(feature_data + 1));
		teeOperation.params[3].tmpref.size =
			sizeof(SENSOR_WINSIZE_INFO_STRUCT);
		break;
	case SENSOR_FEATURE_SET_HDR:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(UINT32);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size =
			sizeof(unsigned long long)*3;
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INOUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_OUTPUT);
		teeOperation.params[1].tmpref.size =
			sizeof(unsigned long long)*2;
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		teeOperation.params[3].tmpref.buffer =
			(void *)(uintptr_t) (*(feature_data + 1));
		teeOperation.params[3].tmpref.size =
			sizeof(SENSOR_VC_INFO_STRUCT);
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size =
			sizeof(SET_SENSOR_AWB_GAIN);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:

		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INOUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_OUTPUT);
		teeOperation.params[1].tmpref.size =
			sizeof(unsigned long long)*2;
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		teeOperation.params[3].tmpref.buffer =
			(void *)(uintptr_t) (*(feature_data + 1));
		teeOperation.params[3].tmpref.size =
			sizeof(SET_PD_BLOCK_INFO_T);
		break;

	case SENSOR_FEATURE_SET_PDAF:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(UINT16);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_OUTPUT,
			TEEC_MEMREF_TEMP_INOUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(UINT32);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INOUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size =
			sizeof(UINT32)*(*feature_para_len);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_OUTPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size =
			sizeof(UINT32)*(*feature_para_len);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);

		break;

	case SENSOR_FEATURE_SET_PDFOCUS_AREA:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size =
			sizeof(unsigned long long)*2;
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;

	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(unsigned long long);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INPUT,
			TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
		teeOperation.params[1].tmpref.size = sizeof(unsigned long long);
		teeOperation.params[2].tmpref.size = sizeof(UINT32);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
	default:
		cam_pr_debug("unhandled feature_id = %d\n", feature_id);
		return TEEC_SUCCESS;
	}


	ret_tee = TEEC_InvokeCommand(&imgsensor_tci_session,
		IMGSENSOR_TEE_CMD_FEATURE_CONTROL, &teeOperation, NULL);
	if (ret_tee == TEEC_SUCCESS)
		*ret = teeOperation.params[0].value.a;
	_TRACE("%s exit\n", __func__);
	return ret_tee;

}


TEEC_Result _tee_cmd_get_resolution(
	struct command_params params, MUINT32 *ret)
{
	TEEC_Result ret_tee = TEEC_SUCCESS;
	struct TEEC_Operation teeOperation;

	_TRACE("%s\n", __func__);
	memset(&teeOperation, 0, sizeof(struct TEEC_Operation));

	teeOperation.params[0].value.a = 0xff;
	teeOperation.paramTypes =
		TEEC_PARAM_TYPES(TEEC_VALUE_INOUT,
				 TEEC_MEMREF_TEMP_OUTPUT,/*get = output*/
				 TEEC_NONE,
				 TEEC_NONE);

	teeOperation.params[1].tmpref.buffer = params.param0;

	teeOperation.params[1].tmpref.size =
		sizeof(MSDK_SENSOR_RESOLUTION_INFO_STRUCT);


	ret_tee = TEEC_InvokeCommand(&imgsensor_tci_session,
		IMGSENSOR_TEE_CMD_GET_RESOLUTION, &teeOperation, NULL);


	cam_pr_debug("cagr: params.param0 = %p\n", params.param0);
	if (ret_tee == TEEC_SUCCESS)
		*ret = teeOperation.params[0].value.a;
	_TRACE("%s exit\n", __func__);
	return ret_tee;
}

TEEC_Result _tee_cmd_getInfo(struct command_params params, MUINT32 *ret)
{

	TEEC_Result ret_tee = TEEC_SUCCESS;
	struct TEEC_Operation teeOperation;
	enum MSDK_SCENARIO_ID_ENUM scenario_id =
			(enum MSDK_SCENARIO_ID_ENUM)(params.param0);

	_TRACE("%s\n", __func__);
	memset(&teeOperation, 0, sizeof(struct TEEC_Operation));
	teeOperation.params[0].value.a = 0xff;
	teeOperation.params[0].value.b = scenario_id;
	teeOperation.paramTypes =
		TEEC_PARAM_TYPES(TEEC_VALUE_INOUT,
				 TEEC_MEMREF_TEMP_OUTPUT,
				 TEEC_MEMREF_TEMP_OUTPUT,
				 TEEC_NONE);

	teeOperation.params[1].tmpref.buffer = params.param1;
	teeOperation.params[1].tmpref.size   =
		sizeof(MSDK_SENSOR_INFO_STRUCT);

	teeOperation.params[2].tmpref.buffer = params.param2;
	teeOperation.params[2].tmpref.size =
		sizeof(MSDK_SENSOR_CONFIG_STRUCT);

	ret_tee = TEEC_InvokeCommand(&imgsensor_tci_session,
		IMGSENSOR_TEE_CMD_GET_INFO, &teeOperation, NULL);

	cam_pr_debug(
	    "cagi: params.param1 = %p param2 = %p sizeof(MSDK_SENSOR_CONFIG_STRUCT) %zu %zu\n",
	    params.param1, params.param2,
	    sizeof(MSDK_SENSOR_CONFIG_STRUCT),
	    sizeof(enum ACDK_SENSOR_MIPI_LANE_NUMBER_ENUM));

	if (ret_tee == TEEC_SUCCESS)
		*ret = teeOperation.params[0].value.a;
	_TRACE("%s exit\n", __func__);
	return ret_tee;
}



TEEC_Result _tee_cmd_Control(struct command_params params, MUINT32 *ret)
{

	TEEC_Result ret_tee = TEEC_SUCCESS;
	struct TEEC_Operation teeOperation;
	enum MSDK_SCENARIO_ID_ENUM scenario_id =
		(enum MSDK_SCENARIO_ID_ENUM)(params.param0);

	_TRACE("%s\n", __func__);
	memset(&teeOperation, 0, sizeof(struct TEEC_Operation));
		teeOperation.params[0].value.a = 0xff;
		teeOperation.params[0].value.b = scenario_id;
	teeOperation.paramTypes =
		TEEC_PARAM_TYPES(TEEC_VALUE_INOUT,
				 TEEC_MEMREF_TEMP_INPUT,
				 TEEC_MEMREF_TEMP_INPUT,
				 TEEC_NONE);

		teeOperation.params[1].tmpref.buffer = params.param1;
		teeOperation.params[1].tmpref.size   =
			sizeof(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT);

		teeOperation.params[2].tmpref.buffer = params.param2;
		teeOperation.params[2].tmpref.size =
			sizeof(MSDK_SENSOR_CONFIG_STRUCT);

	cam_pr_debug("cacon: params.param1 = %p param2 = %p\n",
		params.param1, params.param2);

	ret_tee = TEEC_InvokeCommand(&imgsensor_tci_session,
		IMGSENSOR_TEE_CMD_CONTROL, &teeOperation, NULL);

	if (ret_tee == TEEC_SUCCESS)
		*ret = teeOperation.params[0].value.a;
	_TRACE("%s exit\n", __func__);
	return ret_tee;
}



TEEC_Result _tee_cmd_open(
	struct command_params parms, MUINT32 *ret)
{
	TEEC_Result ret_tee = TEEC_SUCCESS;
	struct TEEC_Operation teeOperation;

	_TRACE("%s\n", __func__);
	memset(&teeOperation, 0, sizeof(struct TEEC_Operation));
	teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	teeOperation.params[0].value.a = 0xff;

	ret_tee = TEEC_InvokeCommand(&imgsensor_tci_session,
			IMGSENSOR_TEE_CMD_OPEN, &teeOperation, NULL);
	if (ret_tee == TEEC_SUCCESS)
		*ret = teeOperation.params[0].value.a;
	_TRACE("%s exit\n", __func__);
	return ret_tee;
}
TEEC_Result _tee_cmd_close(
	struct command_params parms, MUINT32 *ret)
{
	TEEC_Result ret_tee = TEEC_SUCCESS;
	struct TEEC_Operation teeOperation;

	_TRACE("%s\n", __func__);
	memset(&teeOperation, 0, sizeof(struct TEEC_Operation));
	teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	teeOperation.params[0].value.a = 0xff;

	ret_tee = TEEC_InvokeCommand(&imgsensor_tci_session,
			IMGSENSOR_TEE_CMD_CLOSE, &teeOperation, NULL);
	if (ret_tee == TEEC_SUCCESS)
		*ret = teeOperation.params[0].value.a;
	_TRACE("%s exit\n", __func__);
	return ret_tee;
}

TEEC_Result _tee_cmd_dump_reg(
	struct command_params parms, MUINT32 *ret)
{
	TEEC_Result ret_tee = TEEC_SUCCESS;
	struct TEEC_Operation teeOperation;

	_TRACE("%s\n", __func__);
	memset(&teeOperation, 0, sizeof(struct TEEC_Operation));
	teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	teeOperation.params[0].value.a = 0xff;

	ret_tee = TEEC_InvokeCommand(&imgsensor_tci_session,
			IMGSENSOR_TEE_CMD_DUMP_REG, &teeOperation, NULL);
	if (ret_tee == TEEC_SUCCESS)
		*ret = teeOperation.params[0].value.a;
	_TRACE("%s exit\n", __func__);
	return ret_tee;
}

TEEC_Result _tee_cmd_get_i2c_bus(
	struct command_params parms, MUINT32 *ret)
{
	TEEC_Result ret_tee = TEEC_SUCCESS;
	struct TEEC_Operation teeOperation;

	_TRACE("%s\n", __func__);
	memset(&teeOperation, 0, sizeof(struct TEEC_Operation));
	teeOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	teeOperation.params[0].value.a = 0xff;

	ret_tee = TEEC_InvokeCommand(&imgsensor_tci_session,
			IMGSENSOR_TEE_CMD_GET_I2C_BUS, &teeOperation, NULL);
	secure_i2c_bus = teeOperation.params[0].value.b;

	if (ret_tee == TEEC_SUCCESS)
		*ret = teeOperation.params[0].value.a;
	_TRACE("%s exit\n", __func__);
	return ret_tee;
}

TEEC_Result _tee_cmd_set_sensor(
	struct command_params parms, MUINT32 *ret)
{
	TEEC_Result ret_tee = TEEC_SUCCESS;
	struct TEEC_Operation teeOperation;

	_TRACE("%s\n", __func__);
	memset(&teeOperation, 0, sizeof(struct TEEC_Operation));
	teeOperation.paramTypes =
		TEEC_PARAM_TYPES(TEEC_VALUE_INOUT,
				 TEEC_NONE,
				 TEEC_NONE,
				 TEEC_NONE);

	teeOperation.params[0].value.a = 0xff;
	teeOperation.params[0].value.b = (MUINT32)(uintptr_t)parms.param0;

	ret_tee = TEEC_InvokeCommand(&imgsensor_tci_session,
			IMGSENSOR_TEE_CMD_SET_SENSOR, &teeOperation, NULL);

	if (ret_tee == TEEC_SUCCESS)
		*ret = teeOperation.params[0].value.a;

	_TRACE("%s exit\n", __func__);
	return ret_tee;

}


unsigned int  imgsensor_ca_invoke_command(
	enum IMGSENSOR_TEE_CMD cmd, struct command_params parms, MUINT32 *ret)
{
	TEEC_Result ret_tee = TEEC_SUCCESS;

	_TRACE("%s cmd %d\n", __func__, cmd);
	if (atomic_read(&ca_open_cnt) == 0) {
		cam_pr_info("%s CA TA sessoin not init yet, cmd %d\n",
			__func__, cmd);
		return TEEC_ERROR_GENERIC;
	}

		switch (cmd) {
		case IMGSENSOR_TEE_CMD_OPEN:
			ret_tee = _tee_cmd_open(parms, ret);
			break;
		case IMGSENSOR_TEE_CMD_CLOSE:
			ret_tee = _tee_cmd_close(parms, ret);
			break;
		case IMGSENSOR_TEE_CMD_FEATURE_CONTROL:
			ret_tee = _tee_cmd_feature_control(parms, ret);
			break;
		case IMGSENSOR_TEE_CMD_CONTROL:
			ret_tee = _tee_cmd_Control(parms, ret);
			break;
		case IMGSENSOR_TEE_CMD_GET_INFO:
			ret_tee =  _tee_cmd_getInfo(parms, ret);
			break;
		case IMGSENSOR_TEE_CMD_GET_RESOLUTION:
			ret_tee = _tee_cmd_get_resolution(parms, ret);
			break;
		case IMGSENSOR_TEE_CMD_DUMP_REG:
			ret_tee = _tee_cmd_dump_reg(parms, ret);
			break;
		case IMGSENSOR_TEE_CMD_GET_I2C_BUS:
			ret_tee = _tee_cmd_get_i2c_bus(parms, ret);
			break;
		case IMGSENSOR_TEE_CMD_SET_SENSOR:
			ret_tee = _tee_cmd_set_sensor(parms, ret);
			break;
		default:
			break;
		}

	if (ret_tee != TEEC_SUCCESS) {
		cam_pr_info("%s cmd %d parms[0] %p, failed (0x%08x), %d\n",
			 __func__,
			cmd, parms.param0, ret_tee, *ret);
		return ret_tee;
	}
	cam_pr_debug("%s exit (0x%08x), %d cmd %d",
		__func__,
		ret_tee,
		*ret, cmd);
	return ret_tee;
}
