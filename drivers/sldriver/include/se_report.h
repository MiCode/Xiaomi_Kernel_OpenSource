/**
 *
 * INTEL CONFIDENTIAL
 * Copyright 2011, 2012, 2013 Intel Corporation All Rights Reserved. 
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and treaty
 * provisions. No part of the Material may be used, copied, reproduced, modified,
 * published, uploaded, posted, transmitted, distributed, or disclosed in any
 * way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */


/*
 *  This file is to define Enclave's Report
*/
#pragma once

#ifndef _SE_REPORT_H_
#define _SE_REPORT_H_

#include "se_attributes.h"
#include "se_key.h"

#define SE_HASH_SIZE        32              // SHA256
#define SE_MAC_SIZE         16              // Message Authentication Code - 16 bytes

#define REPORT_DATA_SIZE    64

typedef uint8_t             se_measurement_t[SE_HASH_SIZE];
typedef uint8_t             se_mac_t[SE_MAC_SIZE];
typedef uint8_t             se_report_data_t[REPORT_DATA_SIZE];
typedef uint16_t            se_prod_id_t;

typedef struct _targe_info_t {
    se_measurement_t        mr_enclave;     // (  0) The MRENCLAVE of the target enclave
    se_attributes_t         attributes;     // ( 32) The ATTRIBUTES field of the target enclave
} se_target_info_t;

typedef struct _report_body_t {
    se_cpu_svn_t            cpu_svn;        // (  0) Security Version of the CPU
    uint8_t                 reserved1[32];  // ( 16)
    se_attributes_t         attributes;     // ( 48) Any special Capabilities the Enclave possess
    se_measurement_t        mr_enclave;     // ( 64) The value of the enclave's ENCLAVE measurement
    uint8_t                 reserved2[32];  // ( 96)
    se_measurement_t        mr_signer;      // (128) The value of the enclave's SIGNER measurement
    uint8_t                 reserved3[32];  // (160)
    se_measurement_t        mr_reserved1;   // (192) Reserved measurement for future use
    se_measurement_t        mr_reserved2;   // (224) Reserved measurement for future use
    se_prod_id_t            isv_prod_id;    // (256) Product ID of the Enclave
    se_isv_svn_t            isv_svn;        // (258) Security Version of the Enclave
    uint8_t                 reserved4[60];  // (260) Must be zero
    se_report_data_t        report_data;    // (320) Data provided by the user
} se_report_body_t;

typedef struct _report_t {                  // 432 bytes
    se_report_body_t           body;
    se_key_id_t                key_id;      // (384) KeyID used for diversifying the key tree
    se_mac_t                   mac;         // (416) The Message Authentication Code over this structure.
} se_report_t;

#endif
