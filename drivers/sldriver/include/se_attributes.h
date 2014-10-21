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
#pragma once

#ifndef _SE_ATTRIBUTES_H_
#define _SE_ATTRIBUTES_H_

//#include <stdint.h>

// Enclave Flags Bit Masks
#define SE_FLAGS_INITTED        0x0000000000000001ULL     // if set, then the enclave is initialized
#define SE_FLAGS_DEBUG          0x0000000000000002ULL     // if set, then the enclave is debug
#define SE_FLAGS_MODE64BIT      0x0000000000000004ULL     // if set, then the enclave is 64 bit
#define SE_FLAGS_PROVISION_KEY  0x0000000000000010ULL     // if set, then the enclave has access to provision key
#define SE_FLAGS_LICENSE_KEY    0x0000000000000020ULL     // if set, then the enclave has access to License key
#define SE_FLAGS_RESERVED       (~(SE_FLAGS_INITTED | SE_FLAGS_DEBUG | SE_FLAGS_MODE64BIT | SE_FLAGS_PROVISION_KEY | SE_FLAGS_LICENSE_KEY))

// XSAVE Feature Request Mask
#define SE_XFRM_LEGACY          0x0000000000000003ULL
#define SE_XFRM_AVX             0x0000000000000006ULL
#define SE_XFRM_AVX3            0x00000000000000E6ULL
#define SE_XFRM_LWP             0x4000000000000000ULL
#define SE_XFRM_MPX             0x0000000000000018ULL
#define SE_XFRM_RESERVED        (~(SE_XFRM_LEGACY | SE_XFRM_AVX | SE_XFRM_AVX3 | SE_XFRM_MPX))

typedef struct _se_attributes_t
{
    uint64_t      flags;
    uint64_t      xfrm;
} se_attributes_t;


#endif//_SE_ATTRIBUTES_H_
