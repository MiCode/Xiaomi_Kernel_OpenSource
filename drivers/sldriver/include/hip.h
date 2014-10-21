/* Copyright (C) 2006 Intel Corp.
 *
 * The source code contained or described herein and all documents
 * related to the source code ("Material") are owned by Intel
 * Corporation or its suppliers and licensors. Title to the Material
 * remains with Intel Corporation or its suppliers and licensors. The
 * Material contains trade secrets and proprietary and confidential
 * information of Intel or its suppliers and licensors. The Material
 * is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied,
 * reproduced, modified, published, uploaded, posted, transmitted,
 * distributed, or disclosed in any way without Intel's prior express
 * written permission.
 *
 * Unless otherwise expressly permitted by Intel in a separate license
 * agreement, use of the Material is subject to the copyright notices,
 * trademarks, warranty, use, and disclosure restrictions reflected on
 * the outside of the media, in the documents themselves, and in the
 * "About" or "Read Me" or similar file contained within this source
 * code, and identified as (name of the file) . Unless otherwise
 * expressly agreed by Intel in writing, you may not remove or alter
 * such notices in any way.
 *
 *********************************************************************/

#ifndef __INCLUDE__HIP_H__
#define __INCLUDE__HIP_H__

//#include "stddef.h"
#include <linux/types.h>

/* Hypervisor Information Page */
struct hip_t
{
        uint32_t  _runtime_version;               // 0x0
        uint32_t  _hip_identifier;
        uint16_t  _api_revision;                  // 0x8
        uint16_t  _api_version;
        uint16_t  _kernel_revision;
        uint16_t  _kernel_version;
        uint32_t  _feature_lt             : 1;    // 0x10
        uint32_t  _feature_em             : 1;
        uint32_t  _feature_pm             : 1;
        uint32_t  _feature_px             : 1;
        uint32_t  _feature_vtd            : 1;
        uint32_t  _feature_apic_pass_through : 1;
        uint32_t  _feature_eptad          : 1;
        uint32_t                          : 25;
        uint32_t  _feature_perf_sample    : 1;
        uint32_t  _feature_perf_trace     : 1;
        uint32_t  _feature_perf_path      : 1;
        uint32_t                          : 28;
        uint32_t  _feature_cachesim       : 1;
        uint64_t                          : 64;   // 0x18
        uint64_t                          : 64;   // 0x20
        uint16_t                          : 16;   // 0x28
        uint8_t   _num_cpus;
        uint8_t                           :  8;
        uint32_t                          : 32;
        uint64_t  _reserved[506];                 // 0x30
	uint32_t  valid_identifier;// = 0x48564950
};
        
bool valid(struct hip_t* );

uint16_t kernel_version(struct hip_t*);

uint16_t kernel_revision(struct hip_t*); 

uint16_t api_version(struct hip_t*) ;

uint16_t api_revision(struct hip_t*) ;

bool feature_em(struct hip_t*) ;

bool feature_perf_sample(struct hip_t* ) ;

bool feature_perf_trace(struct hip_t* );

bool feature_perf_path( struct hip_t* );

bool feature_eptad( struct hip_t* );

bool feature_cachesim( struct hip_t*);

unsigned num_cpus( struct hip_t* );



#endif  // !__INCL
