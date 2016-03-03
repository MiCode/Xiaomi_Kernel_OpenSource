/*
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PERF_EVENT_KRYO_H
#define __PERF_EVENT_KRYO_H

#define pmactlr_el0    "S3_5_c11_c0_2"
#define pmresr0l_el0   "S3_5_c11_c3_0"
#define pmresr0h_el0   "S3_5_c11_c3_1"
#define pmresr1l_el0   "S3_5_c11_c3_2"
#define pmresr1h_el0   "S3_5_c11_c3_3"
#define pmresr2l_el0   "S3_5_c11_c3_4"
#define pmresr2h_el0   "S3_5_c11_c3_5"
#define pmxevcntcr_el0 "S3_5_c11_c0_3"

#define RESR_L                         0
#define RESR_H                         1
#define RESR_ENABLE           0x80000000

#define ARMV8_PMCR_P          0x00000002 /* Reset counters */
#define ARMV8_PMCR_C          0x00000004 /* Reset cycle counter */

#define PMACTLR_UEN           0x00000001
#define PMUSERENR_UEN         0x00000001

/*
   event encoding:                 NRCCG
   n  = prefix (1 for Kryo CPU)
   r  = register
   cc = code
   g  = group
*/
#define KRYO_EVT_PREFIX                1
#define KRYO_EVT_MASK         0x000FFFFF
#define KRYO_EVT_PREFIX_MASK  0x000F0000
#define KRYO_EVT_REG_MASK     0x0000F000
#define KRYO_EVT_CODE_MASK    0x00000FF0
#define KRYO_EVT_GROUP_MASK   0x0000000F
#define KRYO_EVT_PREFIX_SHIFT         16
#define KRYO_EVT_REG_SHIFT            12
#define KRYO_EVT_CODE_SHIFT            4
#define KRYO_EVT_GROUP_SHIFT           0
#define KRYO_MODE_EXCL_MASK   0xC0000000

#define KRYO_MAX_L1_REG                2
#define KRYO_MAX_GROUP                 7

#endif
