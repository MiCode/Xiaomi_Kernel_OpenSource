// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/string.h>
#include "tee_client_api.h"
#include "tlcDpHdcp.h"

#define LOG_TAG "DP_HDCP_CA"

#define AN_LEN 8
#define AKSV_LEN 5
#define DEFAULT_WRITE_VAL_LEN 1
#define DEFAULT_WRITE_VAL 0

//key id used in keyinstall
#define HDCP_1X_TX_ID 2
#define HDCP_2X_TX_ID 3
#define TZCMD_DRMKEY_LOAD_HDCPKEY 12

const struct TEEC_UUID uuid = {
	0xabcd270e, 0xa5c4, 0x4c58,
	{0xbc, 0xd3, 0x38, 0x4a, 0x2f, 0xa2, 0x53, 0x9e}
};

const struct TEEC_UUID KI_TA_UUID = {
	0x08110000, 0x0000, 0x0000,
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

const static size_t MAX_SHARED_MEMORY_SIZE = 4 * 1024; // 4K
static struct TEEC_SharedMemory sSharedMemoryMain;

struct TEEC_Context sContext;
struct TEEC_Session sSession;
static bool g_init;

int tee_addDevice(uint32_t version)
{
	uint32_t ret = TEEC_SUCCESS;
	struct tci_t *tci;
	struct TEEC_Operation op;
	bool needLoadKey = false;

	if (g_init)
		tee_removeDevice();

#ifndef HDCP_SETKEY_FROM_KERNEL
	//load key first
	if (version > 0) {
		ret = tee_loadHdcpKeyById(version);
		if (ret != TEEC_SUCCESS) {
			TLCERR("ret = 0x%x", ret);
			return ret;
		}
		needLoadKey = true;
	}
#endif

	ret = TEEC_InitializeContext(NULL, &sContext);
	if (ret != TEEC_SUCCESS) {
		TLCERR("ret = 0x%x", ret);
		return ret;
	}

	ret = TEEC_OpenSession(&sContext, &sSession, &uuid, TEEC_LOGIN_PUBLIC,
			NULL, NULL, NULL);
	if (ret != TEEC_SUCCESS) {
		TLCERR("TEEC_OpenSession ret = 0x%x", ret);
		TEEC_FinalizeContext(&sContext);
		return ret;
	}

	// Try to register one 4M main share memory, if failed(means current
	// tee don't support), register four 1M extra share memory instead.
	// Only do this when first initialize.
	sSharedMemoryMain.buffer = NULL;
	sSharedMemoryMain.size = MAX_SHARED_MEMORY_SIZE;
	sSharedMemoryMain.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	ret = TEEC_AllocateSharedMemory(&sContext, &sSharedMemoryMain);
	if (ret == TEEC_SUCCESS) {
		TLCINFO("Register 4k share memory successfully, (%p)",
			sSharedMemoryMain.buffer);
	} else {
		TLCERR("Register 4k share memory failed(0x%x), (%p)",
			ret, sSharedMemoryMain.buffer);
		if (sSharedMemoryMain.buffer != NULL) {
			TEEC_ReleaseSharedMemory(&sSharedMemoryMain);
			sSharedMemoryMain.buffer = NULL;
		}
	}

	// Copy parameter for add new device
	tci = (struct tci_t *)sSharedMemoryMain.buffer;
	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_DEVICE_ADDED;
	tci->message.cmdHDCP.cmdBody.cmdHDCPInitForVerion.version = version;
	tci->message.cmdHDCP.cmdBody.cmdHDCPInitForVerion.needLoadKey =
		needLoadKey;

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH;
	ret = TEEC_InvokeCommand(&sSession, CMD_DEVICE_ADDED, &op, NULL);
	if (ret != TEEC_SUCCESS) {
		TLCERR("InvokeCommand ret = 0x%x", ret);
		tee_removeDevice();
	}

	g_init = true;

	return ret;
}

void tee_removeDevice(void)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameter for add new device
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;

	if (!g_init)
		return;

	g_init = false;
	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_DEVICE_REMOVE;

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH;
	ret = TEEC_InvokeCommand(&sSession, CMD_DEVICE_REMOVE, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("ret = 0x%x", ret);

	//Release all resources
	if (sSharedMemoryMain.buffer != NULL) {
		TEEC_ReleaseSharedMemory(&sSharedMemoryMain);
		sSharedMemoryMain.buffer = NULL;
	}

	TEEC_CloseSession(&sSession);
	TEEC_FinalizeContext(&sContext);
}

int tee_clearParing(void)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameter for add new device
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_DEVICE_CLEAN;

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH;
	ret = TEEC_InvokeCommand(&sSession, CMD_DEVICE_CLEAN, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("ret = 0x%x", ret);

	return ret;
}

int tee_hdcp1x_setTxAn(uint8_t *an_code)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	uint8_t an_len = 8;
	// Copy parameter for add new device
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;
	uint8_t *share_buffer = (uint8_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_WRITE_VAL;
	tci->message.cmdHDCP.cmdBody.cmdHDCPWriteVal.len = an_len;
	tci->message.cmdHDCP.cmdBody.cmdHDCPWriteVal.type = TYPE_HDCP_PARAM_AN;
	memcpy(share_buffer + TCI_LENGTH, an_code, an_len);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH + an_len;
	ret = TEEC_InvokeCommand(&sSession, CMD_WRITE_VAL, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("ret = 0x%x", ret);

	return ret;
}

int tee_hdcp_enableEncrypt(bool bEnable, uint8_t version)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameter for add new device
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;
	uint8_t *share_buffer = (uint8_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_ENABLE_ENCRYPT;
	if (bEnable)
		tci->message.cmdHDCP.cmdBody.cmdHDCPWriteVal.type
			= TYPE_HDCP_ENABLE_ENCRYPT;
	else
		tci->message.cmdHDCP.cmdBody.cmdHDCPWriteVal.type
			= TYPE_HDCP_DISABLE_ENCRYPT;

	//set HDCP version supportted by device
	tci->message.cmdHDCP.cmdBody.cmdHDCPWriteVal.len = 1;
	memset(share_buffer + TCI_LENGTH, version, 1);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH + 1;
	ret = TEEC_InvokeCommand(&sSession, CMD_ENABLE_ENCRYPT, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("ret = 0x%x", ret);

	return ret;
}

int tee_hdcp1x_softRst(void)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameter for add new device
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;
	uint8_t *share_buffer = (uint8_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_WRITE_VAL;
	tci->message.cmdHDCP.cmdBody.cmdHDCPWriteVal.type
		= TYPE_HDCP_PARAM_RST_1;
	//No need input. Set default value 0 for check
	tci->message.cmdHDCP.cmdBody.cmdHDCPWriteVal.len =
		DEFAULT_WRITE_VAL_LEN;
	memset(share_buffer + TCI_LENGTH, DEFAULT_WRITE_VAL,
		DEFAULT_WRITE_VAL_LEN);
	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH + DEFAULT_WRITE_VAL_LEN;
	ret = TEEC_InvokeCommand(&sSession, CMD_WRITE_VAL, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("ret = 0x%x", ret);

	return ret;
}

int tee_hdcp2_softRst(void)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameter for add new device
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;
	uint8_t *share_buffer = (uint8_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_WRITE_VAL;
	tci->message.cmdHDCP.cmdBody.cmdHDCPWriteVal.type
		= TYPE_HDCP_PARAM_RST_2;
	//No need input. Set default value 0 for check
	tci->message.cmdHDCP.cmdBody.cmdHDCPWriteVal.len =
		DEFAULT_WRITE_VAL_LEN;
	memset(share_buffer + TCI_LENGTH, DEFAULT_WRITE_VAL,
		DEFAULT_WRITE_VAL_LEN);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH + DEFAULT_WRITE_VAL_LEN;
	ret = TEEC_InvokeCommand(&sSession, CMD_WRITE_VAL, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("ret = 0x%x", ret);

	return ret;
}

/** V1.X **/
int tee_getAksv(uint8_t *Aksv)
{
	uint32_t ret = TEEC_SUCCESS;
	struct tci_t *tci;
	struct TEEC_Operation op;

	// Copy parameters
	tci = (struct tci_t *)sSharedMemoryMain.buffer;
	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_GET_AKSV;
	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH;
	ret = TEEC_InvokeCommand(&sSession, CMD_GET_AKSV, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("InvokeCommand ret = 0x%x", ret);

	memcpy(Aksv, tci->message.cmdHDCP.cmdBody.cmdHDCPGetAksv.aksv,
		AKSV_LEN);

	return ret;
}
int tee_calculateLm(uint8_t *Bksv)
{
	uint32_t ret = TEEC_SUCCESS;
	struct tci_t *tci;
	struct TEEC_Operation op;

	// Copy parameters
	tci = (struct tci_t *)sSharedMemoryMain.buffer;
	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_CALCULATE_LM;
	memcpy(tci->message.cmdHDCP.cmdBody.cmdHDCPCalculateLm.bksv, Bksv,
		BKSV_LEN);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH;
	ret = TEEC_InvokeCommand(&sSession, CMD_CALCULATE_LM, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("InvokeCommand ret = 0x%x", ret);

	return ret;
}

int tee_compareR0(uint8_t *r0, uint32_t len)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameters
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;
	uint8_t *share_buffer = (uint8_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_COMPARE_R0;
	tci->message.cmdHDCP.cmdBody.cmdHDCPCompare.rx_val_len = len;
	memcpy(share_buffer + TCI_LENGTH, r0, len);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH + len;
	ret = TEEC_InvokeCommand(&sSession, CMD_COMPARE_R0, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("InvokeCommand ret = 0x%x", ret);

	return ret;
}

int tee_hdcp1x_ComputeCompareV(uint8_t *cryptoParam, uint32_t paramLen,
	uint8_t *rxV)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameters
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;
	uint8_t *share_buffer = (uint8_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_COMPARE_V1;
	tci->message.cmdHDCP.cmdBody.cmdHDCPCompare.rx_val_len = 20;
	tci->message.cmdHDCP.cmdBody.cmdHDCPCompare.param_len = paramLen;
	memcpy(share_buffer + TCI_LENGTH, rxV, 20);
	memcpy(share_buffer + TCI_LENGTH + 20, cryptoParam, paramLen);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH + paramLen + 20;
	ret = TEEC_InvokeCommand(&sSession, CMD_COMPARE_V1, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("InvokeCommand ret = 0x%x", ret);

	return ret;
}

/**** V2.X****/
int tee_akeCertificate(uint8_t *certificate, bool *bStored, uint8_t *out_m,
	uint8_t *out_ekm)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameters
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_AKE_CERTIFICATE;
	memcpy(tci->message.cmdHDCP.cmdBody.cmdHDCPAKECertificate.certification,
		certificate, CERT_LEN);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH;
	ret = TEEC_InvokeCommand(&sSession, CMD_AKE_CERTIFICATE, &op, NULL);
	if (ret != TEEC_SUCCESS) {
		TLCERR("InvokeCommand ret = 0x%x", ret);
		return ret;
	}

	TLCINFO("verify signature: result %d", ret);
	*bStored = tci->message.cmdHDCP.cmdBody.cmdHDCPAKECertificate.bStored;
	memcpy(out_m, tci->message.cmdHDCP.cmdBody.cmdHDCPAKECertificate.m,
		M_LEN);
	memcpy(out_ekm, tci->message.cmdHDCP.cmdBody.cmdHDCPAKECertificate.ekm,
		EKM_LEN);

	return ret;
}

int tee_encRsaesOaep(uint8_t *ekm)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameters
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_ENC_KM;

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH;
	ret = TEEC_InvokeCommand(&sSession, CMD_ENC_KM, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("InvokeCommand ret = 0x%x", ret);

	memcpy(ekm, tci->message.cmdHDCP.cmdBody.cmdHDCPEncKm.encKm,
		ENC_KM_LEN);
	return ret;
}

int tee_akeHPrime(uint8_t *rtx, uint8_t *rrx, uint8_t *rxCaps, uint8_t *txCaps,
	 uint8_t *rxH, uint32_t rxH_len)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameters
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;
	uint8_t *share_buffer = (uint8_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_AKE_H_PRIME;
	tci->message.cmdHDCP.cmdBody.cmdHDCPAKEHPrime.rxH_len = rxH_len;

	memcpy(tci->message.cmdHDCP.cmdBody.cmdHDCPAKEHPrime.rtx, rtx, RXX_LEN);
	memcpy(tci->message.cmdHDCP.cmdBody.cmdHDCPAKEHPrime.rrx, rrx, RXX_LEN);
	memcpy(tci->message.cmdHDCP.cmdBody.cmdHDCPAKEHPrime.rxCaps, rxCaps,
		CAPS_LEN);
	memcpy(tci->message.cmdHDCP.cmdBody.cmdHDCPAKEHPrime.txCaps, txCaps,
		CAPS_LEN);
	memcpy(share_buffer + TCI_LENGTH, rxH, rxH_len);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH + rxH_len;
	ret = TEEC_InvokeCommand(&sSession, CMD_AKE_H_PRIME, &op, NULL);
	if (ret != TEEC_SUCCESS) {
		TLCERR("InvokeCommand ret = 0x%x", ret);
		return ret;
	}

	return tci->message.cmdHDCP.responseHeader.returnCode;
}

int tee_akeParing(uint8_t *rx_ekm)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameters
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_AKE_PARING;
	memcpy(tci->message.cmdHDCP.cmdBody.cmdHDCPAKEParing.ekm, rx_ekm,
		EKM_LEN);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH;
	ret = TEEC_InvokeCommand(&sSession, CMD_AKE_PARING, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("InvokeCommand ret = 0x%x", ret);

	return ret;
}

int tee_lcLPrime(uint8_t *rn, uint8_t *rxL, uint32_t len)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameters
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;
	uint8_t *share_buffer = (uint8_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_LC_L_PRIME;
	memcpy(tci->message.cmdHDCP.cmdBody.cmdHDCPLcLPrime.rn, rn, RN_LEN);
	tci->message.cmdHDCP.cmdBody.cmdHDCPLcLPrime.rxL_len = len;
	memcpy(share_buffer + TCI_LENGTH, rxL, len);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH + len;
	ret = TEEC_InvokeCommand(&sSession, CMD_LC_L_PRIME, &op, NULL);
	if (ret != TEEC_SUCCESS) {
		TLCERR("InvokeCommand ret = 0x%x", ret);
		return ret;
	}

	return tci->message.cmdHDCP.responseHeader.returnCode;
}

int tee_skeEncKs(uint8_t *riv, uint8_t *eks)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameters
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;
	uint8_t *share_buffer = (uint8_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_SKE_CAL_EKS;
	memcpy(tci->message.cmdHDCP.cmdBody.cmdHDCPSKEEks.riv, riv, RIV_LEN);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH + 16;
	ret = TEEC_InvokeCommand(&sSession, CMD_SKE_CAL_EKS, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("InvokeCommand ret = 0x%x", ret);

	memcpy(eks, share_buffer + TCI_LENGTH, 16);

	return ret;
}

int tee_hdcp2_ComputeCompareV(uint8_t *cryptoParam, uint32_t paramLen,
	uint8_t *rxV, uint8_t *txV)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameters
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;
	uint8_t *share_buffer = (uint8_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_COMPARE_V2;
	tci->message.cmdHDCP.cmdBody.cmdHDCPCompare.rx_val_len = 16;
	tci->message.cmdHDCP.cmdBody.cmdHDCPCompare.param_len = paramLen;
	memcpy(share_buffer + TCI_LENGTH, rxV, 16);
	memcpy(share_buffer + TCI_LENGTH + 16, cryptoParam, paramLen);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH + paramLen + 16;
	ret = TEEC_InvokeCommand(&sSession, CMD_COMPARE_V2, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("InvokeCommand ret = 0x%x", ret);
	else
		memcpy(txV, share_buffer + TCI_LENGTH, 16);

	return ret;
}

int tee_hdcp2_ComputeCompareM(uint8_t *cryptoParam, uint32_t paramLen,
	uint8_t *rxM)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameters
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;
	uint8_t *share_buffer = (uint8_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_COMPARE_M;
	tci->message.cmdHDCP.cmdBody.cmdHDCPCompare.rx_val_len = 32;
	tci->message.cmdHDCP.cmdBody.cmdHDCPCompare.param_len = paramLen;
	memcpy(share_buffer + TCI_LENGTH, rxM, 32);
	memcpy(share_buffer + TCI_LENGTH + 32, cryptoParam, paramLen);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH + paramLen + 32;
	ret = TEEC_InvokeCommand(&sSession, CMD_COMPARE_M, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("InvokeCommand ret = 0x%x", ret);

	return ret;
}

int tee_loadHdcpKeyById(uint32_t version)
{
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation sOperation;
	struct TEEC_Context context;
	struct TEEC_Session session;
	uint32_t keyID = 0;

	switch (version) {
	case HDCP_VERSION_1X:
		keyID = HDCP_1X_TX_ID;
		break;
	case HDCP_VERSION_2X:
		keyID = HDCP_2X_TX_ID;
		break;
	default:
		TLCERR("Version not support %d", version);
		return TEEC_ERROR_NOT_SUPPORTED;
	}

	/* Create Device context  */
	ret = TEEC_InitializeContext(NULL, &context);
	if (ret != TEEC_SUCCESS) {
		TLCERR("load key ret = 0x%x", ret);
		return ret;
	}

	memset(&sOperation, 0, sizeof(struct TEEC_Operation));
	sOperation.paramTypes = 0;
	ret = TEEC_OpenSession(&context,
		&session,               /* OUT session */
		&KI_TA_UUID,            /* destination UUID */
		TEEC_LOGIN_PUBLIC,      /* connectionMethod */
		NULL,                   /* connectionData */
		&sOperation,            /* IN OUT operation */
		NULL                    /* OUT returnOrigin, optional */
	);
	if (ret != TEEC_SUCCESS) {
		TLCERR("load key ret = 0x%x", ret);
		TEEC_FinalizeContext(&context);
		return ret;
	}

	memset(&sOperation, 0, sizeof(struct TEEC_Operation));
	sOperation.paramTypes = TEEC_PARAM_TYPES(
		TEEC_VALUE_INPUT,
		TEEC_NONE,
		TEEC_NONE,
		TEEC_NONE);

	sOperation.params[0].value.a = keyID;

	ret = TEEC_InvokeCommand(&session,
		TZCMD_DRMKEY_LOAD_HDCPKEY,
		&sOperation,        /* IN OUT operation */
		NULL                /* OUT returnOrigin, optional */
	);

	TEEC_CloseSession(&session);
	TEEC_FinalizeContext(&context);
	return ret;
}

/*for test and need remove in furture*/
int tee_hdcp1x_setKey(uint8_t *key)
{
#ifdef HDCP_SETKEY_FROM_KERNEL
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameters
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;
	uint8_t *share_buffer = (uint8_t *)sSharedMemoryMain.buffer;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_LOAD_KEY;
	tci->message.cmdHDCP.cmdBody.cryptokeys.type = TYPE_HDCP13_KEY;
	tci->message.cmdHDCP.cmdBody.cryptokeys.len = 289;
	memcpy(share_buffer + TCI_LENGTH, key, 289);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH + 289;
	ret = TEEC_InvokeCommand(&sSession, CMD_LOAD_KEY, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("InvokeCommand ret = 0x%x", ret);

	return ret;
#else
	return TEEC_SUCCESS;
#endif
}

int tee_hdcp2_setKey(uint8_t *kpubdcp, uint8_t *lc128)
{
#ifdef HDCP_SETKEY_FROM_KERNEL
	uint32_t ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	// Copy parameters
	struct tci_t *tci = (struct tci_t *)sSharedMemoryMain.buffer;
	uint8_t *share_buffer = (uint8_t *)sSharedMemoryMain.buffer;
	uint32_t len = 385 + 16;

	memset(tci, 0, TCI_LENGTH);
	tci->message.cmdHDCP.commandHeader.commandId = CMD_LOAD_KEY;
	tci->message.cmdHDCP.cmdBody.cryptokeys.type = TYPE_HDCP22_KEY;
	tci->message.cmdHDCP.cmdBody.cryptokeys.len = len;
	memcpy(share_buffer + TCI_LENGTH, kpubdcp, 385);
	memcpy(share_buffer + TCI_LENGTH + 385, lc128, 16);

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE,
		TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sSharedMemoryMain;
	op.params[0].memref.size = TCI_LENGTH + len;
	ret = TEEC_InvokeCommand(&sSession, CMD_LOAD_KEY, &op, NULL);
	if (ret != TEEC_SUCCESS)
		TLCERR("InvokeCommand ret = 0x%x", ret);

	return ret;
#else
	return TEEC_SUCCESS;
#endif
}

