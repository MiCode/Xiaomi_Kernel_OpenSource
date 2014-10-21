/**
 *
 * INTEL CONFIDENTIAL
 * Copyright 2011 Intel Corporation All Rights Reserved. 
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
 *	This file is to define Enclave's keys
*/
#pragma once

#ifndef _SE_KEY_H_
#define _SE_KEY_H_

//#include <stdint.h>
#include "se_attributes.h"

// Key Name
#define KEYSELECT_LICENSE          0x0000
#define KEYSELECT_PROVISION        0x0001
#define KEYSELECT_PROVISION_SEAL   0x0002
#define KEYSELECT_REPORT           0x0003
#define KEYSELECT_SEAL             0x0004

// Key Policy
#define KEYPOLICY_MRENCLAVE        0x0001      // Derive key using the enclave's ENCLAVE measurement register
#define KEYPOLICY_MRSIGNER         0x0002      // Derive key using the enclave's SINGER measurement register

#define KEY_ID_SIZE                8

#define CPU_SVN_SIZE               16

typedef uint8_t                    se_key_128bit_t[16];
typedef uint32_t                   se_key_id_t[KEY_ID_SIZE];
typedef uint8_t                    se_cpu_svn_t[CPU_SVN_SIZE];
typedef uint16_t                   se_isv_svn_t;


typedef struct _key_request_t {
   uint16_t                        key_name;           // 000 Identifies the key required
   uint16_t                        key_policy;         // 002 Identifies which inputes should be used in thekey derivation
   se_isv_svn_t                    isv_svn;            // 004 Security Version of the Enclave
   uint16_t                        reserved;           // 006 Must be 0
   se_cpu_svn_t                    cpu_svn;            // 008 Security Version of the CPU
   se_attributes_t                 attribute_mask;     // 024 Mask which ATTRIBUTES Seal keys should be bound to
   se_key_id_t                        key_id;             // 040 Value for key wear-out protection
} se_key_request_t;


#endif
