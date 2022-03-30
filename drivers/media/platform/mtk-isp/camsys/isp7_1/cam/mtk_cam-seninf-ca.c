// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include<linux/slab.h>

#include "mtk_cam-seninf-ca.h"

#define PFX "mtk_cam-seninf-ca"
#define LOG_INF(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)
#define PARAM_SIZE 4


#define SRV_NAME(name)   "com.mediatek.geniezone." name

static struct SENINF_CA *pca;

static const char imgsensor_srv_name[] =  SRV_NAME("srv.imgsensor");
#define IMGSENSOR_UUID { 0x7b665120, 0x2121, 0x4487, \
	{ 0xba, 0x71, 0x0a, 0x51, 0xd7, 0xea, 0x78, 0xfe } }

int seninf_ca_open_session(void)
{
	int ret = SENINF_CA_RETURN_SUCCESS;
	TZ_RESULT ret_tz = TZ_RESULT_SUCCESS;

	LOG_INF("[%s] +", __func__);

	pca = kmalloc(sizeof(struct SENINF_CA), GFP_KERNEL);

	memset(pca, 0, sizeof(struct SENINF_CA));
	ret_tz = KREE_CreateSession(imgsensor_srv_name, &pca->session);
	if (ret_tz != TZ_RESULT_SUCCESS) {
		LOG_INF("[%s] CreateSession fail. ret: %d. ", __func__, ret_tz);
		ret = SENINF_CA_RETURN_ERROR;
	}

	LOG_INF("[%s]-", __func__);
	return ret;
}

int seninf_ca_close_session(void)
{
	int ret = SENINF_CA_RETURN_SUCCESS;
	TZ_RESULT ret_tz = TZ_RESULT_SUCCESS;

	if (pca == NULL)
		return ret;

	ret_tz = KREE_CloseSession(pca->session);
	memset(&pca->session, 0, sizeof(KREE_SESSION_HANDLE));

	if (ret_tz != TZ_RESULT_SUCCESS) {
		LOG_INF("[%s] CloseSession fail. ret: %d. ", __func__, ret_tz);
		ret = SENINF_CA_RETURN_ERROR;
	}

	LOG_INF("[%s] -", __func__);
	return ret;
}



int seninf_ca_checkpipe(unsigned int SecInfo_addr)
{

	int types = 0;
	int ret = SENINF_CA_RETURN_SUCCESS;
	TZ_RESULT ret_tz = TZ_RESULT_SUCCESS;
	union MTEEC_PARAM param[PARAM_SIZE];

	LOG_INF("[%s] +", __func__);

	param[0].value.a = ret;
	param[0].value.b = SecInfo_addr;
	types = TZ_ParamTypes4(TZPT_VALUE_OUTPUT, TZPT_NONE, TZPT_NONE, TZPT_NONE);
	ret_tz = KREE_TeeServiceCall(pca->session, SENINF_TEE_CMD_CHECKPIPE, types, param);

	if (ret_tz != TZ_RESULT_SUCCESS && param[0].value.a) {
		LOG_INF("[%s]checkpipe failed.", __func__);
		ret = SENINF_CA_RETURN_ERROR;
	}

	LOG_INF("[%s] -", __func__);
	return ret;
}





int seninf_ca_free(void)
{
	int types = 0;
	int ret = SENINF_CA_RETURN_SUCCESS;
	TZ_RESULT ret_tz = TZ_RESULT_SUCCESS;
	union MTEEC_PARAM param[PARAM_SIZE];

	LOG_INF("[%s] +", __func__);

	if (pca == NULL) {
		LOG_INF("[%s]pca is null", __func__);
		return ret;
	}

	param[0].value.a = ret;
	types = TZ_ParamTypes4(TZPT_VALUE_OUTPUT, TZPT_NONE, TZPT_NONE, TZPT_NONE);
	ret_tz = KREE_TeeServiceCall(pca->session, SENINF_TEE_CMD_FREE, types, param);

	if (ret_tz != TZ_RESULT_SUCCESS && param[0].value.a) {
		LOG_INF("[%s]free failed.", __func__);
		ret = SENINF_CA_RETURN_ERROR;
	}

	LOG_INF("[%s] -", __func__);
	return ret;
}





