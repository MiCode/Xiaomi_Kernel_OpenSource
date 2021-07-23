/*
 * Copyright (c) 2020 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TCI_H__
#define __TCI_H__
typedef uint32_t tciCommandId_t;
typedef uint32_t tciResponseId_t;
typedef uint32_t tciReturnCode_t;
typedef uint32_t TEE_VA_TYPE;

#define RET_COMPARE_PASS 0
#define RET_COMPARE_FAIL 1
#define RET_NEW_DEVICE 2
#define RET_STORED_DEVICE 3

#define AN_LEN 8
#define AKSV_LEN 5
#define BKSV_LEN 5
#define CERT_LEN 522
#define EKM_LEN 16
#define M_LEN 16
#define ENC_KM_LEN 128
#define RXX_LEN 8
#define CAPS_LEN 3
#define RN_LEN 8
#define RIV_LEN 8

#define TYPE_HDCP_PARAM_AN 10
#define TYPE_HDCP_PARAM_RST_1 11
#define TYPE_HDCP_PARAM_RST_2 12
#define TYPE_HDCP_ENABLE_ENCRYPT 13
#define TYPE_HDCP_DISABLE_ENCRYPT 14

//for test
#define TYPE_HDCP13_KEY 20
#define TYPE_HDCP22_KEY 21
struct cryptokeys_t {
	uint8_t type;
	uint32_t len;
	TEE_VA_TYPE key;
};

/**
 * TCI command header.
 */
struct tciCommandHeader_t {
	tciCommandId_t commandId;
};

/**
 * TCI response header.
 */
struct tciResponseHeader_t {
	tciResponseId_t responseId;
	tciReturnCode_t returnCode;
};

struct cmdHDCPInitForVerion_t {
	uint32_t version;
	bool needLoadKey;
};

struct cmdHDCPWriteVal_t {
	uint8_t type;
	uint8_t len;
	TEE_VA_TYPE val;
};

struct cmdHDCPCalculateLm_t {
	uint8_t bksv[BKSV_LEN];
};

struct cmdHDCPGetAksv_t {
	uint8_t aksv[AKSV_LEN];
};

struct cmdHDCPSha1_t {
	uint32_t message_len;
	TEE_VA_TYPE message_addr;
};

struct cmdHDCPAKECertificate_t {
	uint8_t certification[CERT_LEN];
	bool bStored;
	uint8_t m[M_LEN];
	uint8_t ekm[EKM_LEN];
};

struct cmdHDCPAKEParing_t {
	uint8_t ekm[EKM_LEN];
};

struct cmdHDCPEncKm_t {
	uint8_t encKm[ENC_KM_LEN];
};

struct cmdHDCPAKEHPrime_t {
	uint8_t rtx[RXX_LEN];
	uint8_t rrx[RXX_LEN];
	uint8_t rxCaps[CAPS_LEN];
	uint8_t txCaps[CAPS_LEN];
	uint32_t rxH_len;
	TEE_VA_TYPE rxH;
};

struct cmdHDCPLcLPrime_t {
	uint8_t rn[RN_LEN];
	uint32_t rxL_len;
	TEE_VA_TYPE rxL;
};

struct cmdHDCPSKEEks_t {
	uint8_t riv[RIV_LEN];
	uint32_t eks_len;
	TEE_VA_TYPE eks;
};

struct cmdHDCPCompare_t {
	uint32_t rx_val_len;
	TEE_VA_TYPE rx_val;
	uint32_t param_len;
	TEE_VA_TYPE param;
	uint32_t out_len;
	TEE_VA_TYPE out;
};

union tcicmdBody_t {
	/* init with special HDCP version */
	struct cmdHDCPInitForVerion_t cmdHDCPInitForVerion;
	/* write uint32 data to hw */
	struct cmdHDCPWriteVal_t cmdHDCPWriteVal;
	/* get aksv */
	struct cmdHDCPGetAksv_t cmdHDCPGetAksv;
	/* calculate r0 */
	struct cmdHDCPCalculateLm_t cmdHDCPCalculateLm;
	/* generate signature for certificate*/
	struct cmdHDCPAKECertificate_t cmdHDCPAKECertificate;
	/* to store ekm*/
	struct cmdHDCPAKEParing_t cmdHDCPAKEParing;
	/* encrypt km for V2.2 */
	struct cmdHDCPEncKm_t cmdHDCPEncKm;
	/* compute H prime */
	struct cmdHDCPAKEHPrime_t cmdHDCPAKEHPrime;
	/* compute L prime */
	struct cmdHDCPLcLPrime_t cmdHDCPLcLPrime;
	/* compute eks */
	struct cmdHDCPSKEEks_t cmdHDCPSKEEks;
	/* compare */
	struct cmdHDCPCompare_t cmdHDCPCompare;
	/* for test only */
	struct cryptokeys_t cryptokeys;
} __attribute__ ((__packed__));

struct cmdHDCP_t {
	/* request tci header */
	struct tciCommandHeader_t commandHeader;
	/* response tci header */
	struct tciResponseHeader_t responseHeader;
	uint32_t drHandle;
	union tcicmdBody_t cmdBody;
};

/**
 * TCI message data.
 * It's union structure
 */
union tciMessage_t {
	/* request tci include header and other parameters */
	struct cmdHDCP_t cmdHDCP;
};

/**
 * Overall TCI structure.
 */
struct tci_t {
	union tciMessage_t message; /**< TCI message */
};
const static uint64_t TCI_LENGTH = sizeof(struct tci_t);

#endif //__TCI_H__

