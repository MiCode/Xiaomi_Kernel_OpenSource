/*
 * Copyright (c) 2021, Xiaomi, Inc. All rights reserved.
 */

#ifndef __ISPV3_IOPARAM_H__
#define __ISPV3_IOPARAM_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#endif


/* Command Reserved Types */
#define CAM_RESERVED_OPCODE_BASE        0x2000
#define CAM_RESERVED_POWERUP_EX         (CAM_RESERVED_OPCODE_BASE+0x1)

/* PowerSetting Config Value properties */
#define POWER_CFG_VAL_TYPE_MASK         0xF000
#define POWER_CFG_VAL_TYPE_EX           0x2000
#define POWER_CFG_VAL_MASK              0x000F
typedef enum
{
        ISPV3EventExit = 0,
        ISPV3EventMipiRXErr,
        ISPV3EventMax,
} ISPV3ICEventType;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __ISPV3_IOPARAM_H__
