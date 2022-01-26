/*
 * Copyright (c) 2020 MediaTek Inc.
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

#ifndef __TLCDPHDCP_H__
#define __TLCDPHDCP_H__

#include <linux/printk.h>
#include "tci.h"
#include "tlDPHdcpCMD.h"

#define TLCINFO(string, args...) pr_info("[TLC_HDCP]info: "string, ##args)
#define TLCERR(string, args...) pr_info("[TLC_HDCP]line:%d,err:%s:"string,\
	__LINE__, __func__, ##args)

#define RET_ERROR_KEY_INVALID   10
#define RET_SUCCESS 0

/**
 * hdcp version definitions
 */
#define HDCP_NONE                0x0 // No HDCP supported, no secure data path
#define HDCP_V1                  0x1 // HDCP version 1.0
#define HDCP_V2                  0x2 // HDCP version 2.0 Type 1
#define HDCP_V2_1                0x3 // HDCP version 2.1 Type 1
#define HDCP_V2_2                0x4 // HDCP version 2.2 Type 1
#define HDCP_V2_3                0x5 // HDCP version 2.3 Type 1
// Local display only(content required version use only)
#define HDCP_LOCAL_DISPLAY_ONLY  0xf
#define HDCP_NO_DIGITAL_OUTPUT   0xff // No digital output
#define HDCP_DEFAULT             HDCP_NO_DIGITAL_OUTPUT // Default value

#define HDCP_VERSION_1X 1
#define HDCP_VERSION_2X 2

#ifdef __cplusplus
extern "C"
{
#endif

/*
 *Description:
 *  A device connect and do some initializations.
 *
 *Input:
 *  version: HDCP version
 *
 *Returns:
 *  TEEC_SUCCESS success*
 */
int tee_addDevice(uint32_t version);

/*
 *Description:
 *  Device disconnect.
 *
 *Parameters:
 *  N/A
 *
 *Returns:
 *  N/A
 */
void tee_removeDevice(void);

/*
 *Description:
 *  Clearing paring info.
 *
 *Parameters:
 *  N/A
 *
 *Returns:
 *  TEEC_SUCCESS success*
 */
int tee_clearParing(void);

/*
 *Description:
 *  Calculate Km base on Bksv and write it to HW.
 *
 *Input:
 *  Bksv[5] input
 *
 *Returns:
 *  TEEC_SUCCESS success
 */
int tee_calculateLm(uint8_t *Bksv);

/*
 *Description:
 *  Get Aksv from TEE.
 *
 *Output:
 *  Aksv[5]
 *
 *Returns:
 *  TEEC_SUCCESS success
 */
int tee_getAksv(uint8_t *Aksv);

/*
 *Description:
 *  Get R0 from HW and compare to Rx_r0.
 *
 *Parameters:
 *  r0[len] input
 *
 *Returns:
 *  TEEC_SUCCESS success
 */
int tee_compareR0(uint8_t *r0, uint32_t len);

/*
 *Description:
 *  Compute and compare V value.
 *
 *Input:
 *  cryptoParam[paramLen] params used to calculate
 *  rxV[20] V value from Rx
 *
 *Returns:
 *  RET_COMPARE_PASS verify pass
 */
int tee_hdcp1x_ComputeCompareV(uint8_t *cryptoParam, uint32_t paramLen,
	uint8_t *rxV);

/*
 *Description:
 *  Write An to HW.
 *
 *Input:
 *  an_code[8]
 *
 *Returns:
 *  TEEC_SUCCESS success
 */
int tee_hdcp1x_setTxAn(uint8_t *an_code);

/*
 *Description:
 *  Write RST to HW.
 *
 *Returns:
 *  TEEC_SUCCESS success
 */
int tee_hdcp1x_softRst(void);
int tee_hdcp2_softRst(void);

/*
 *Description:
 *  Set enable or disable to HW.
 *
 *Returns:
 *  TEEC_SUCCESS success
 */
int tee_hdcp_enableEncrypt(bool bEnable, uint8_t version);

/*
 *Description:
 *  AKE cetificate verify.
 *
 *Input:
 *  certificate[522]: cert use to calculate
 *output:
 *  bStored: whether be stored before
 *  out_m[16]
 *  out_ekm[16]
 *
 *Returns:
 *  TEEC_SUCCESS success*
 */
int tee_akeCertificate(uint8_t *certificate, bool *bStored,
	uint8_t *out_m, uint8_t *out_ekm);

/*
 *Description:
 *  Encrypt Km.
 *
 *Output:
 *  ekm[128]: encrypted km
 *
 *Returns:
 *  TEEC_SUCCESS success*
 */
int tee_encRsaesOaep(uint8_t *ekm);

/*
 *Description:
 *  Calculate H prime and compare to rxH
 *
 *Input:
 *  rtx[8]
 *  rrx[8]
 *  rxCaps[3]
 *  txCaps[3]
 *  rxH[rxH_len]
 *
 *Returns:
 *  RET_COMPARE_PASS: compare pass
 */
int tee_akeHPrime(uint8_t *rtx, uint8_t *rrx, uint8_t *rxCaps, uint8_t *txCaps,
	uint8_t *rxH, uint32_t rxH_len);

/*
 *Description:
 *  Store paring info.
 *
 *Input:
 *  rx_ekm[16]
 *
 *Returns:
 *  TEEC_SUCCESS success*
 */
int tee_akeParing(uint8_t *rx_ekm);

/*
 *Description:
 *  Calculate L prime and compare.
 *
 *Input:
 *  rn[8]
 *  rxL[len]
 *
 *Returns:
 *  RET_COMPARE_PASS compare pass
 */
int tee_lcLPrime(uint8_t *rn, uint8_t *rxL, uint32_t len);

/*
 *Description:
 *  Encrypt ks
 *  Write contentkey and riv to hw
 *
 *Input:
 *  riv[8]
 *Output:
 *  eks[16]
 *
 *Returns:
 *  TEEC_SUCCESS success*
 */
int tee_skeEncKs(uint8_t *riv, uint8_t *eks);

/*
 *Description:
 *  Calculate and compare V prime for repeater.
 *
 *Input:
 *  cryptoParam[paramLen] params used to calculate
 *  rxV[16] V value from Rx
 *Output:
 *  txV[16]
 *
 *Returns:
 *  TEEC_SUCCESS success*
 */
int tee_hdcp2_ComputeCompareV(uint8_t *cryptoParam, uint32_t paramLen,
	uint8_t *rxV, uint8_t *txV);

/*
 *Description:
 *  Calculate and compare M prime for repeater.
 *
 *Input:
 *  cryptoParam[paramLen] params used to calculate
 *  rxM[32] M value from Rx
 *
 *Returns:
 *  RET_COMPARE_PASS verify pass
 */
int tee_hdcp2_ComputeCompareM(uint8_t *cryptoParam, uint32_t paramLen,
	uint8_t *rxM);

/*
 *Description:
 *  Get key from Keyinstall.
 *
 *Input:
 *  keyID
 *
 *Returns:
 *  TEEC_SUCCESS success*
 */
int tee_loadHdcpKeyById(uint32_t version);

/*for test and need remove in furture*/
int tee_hdcp1x_setKey(uint8_t *key);
int tee_hdcp2_setKey(uint8_t *kpubdcp, uint8_t *lc128);

#ifdef __cplusplus
}
#endif

#endif //__TLCDPHDCP_H__
