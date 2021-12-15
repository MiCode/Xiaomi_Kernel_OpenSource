/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GC08A3_MAIN_OTP_H__
#define __GC08A3_MAIN_OTP_H__

#define GC08A3_I2C_WRITE_ID                0x22

#define GC08A3_FLAG_GROUP1                 0x40
#define GC08A3_FLAG_GROUP2                 0x40
#define GC08A3_FLAG_GROUP3                 0x37

#define GC08A3_FLAG_MODULE1_ADDR            0x15A0
#define GC08A3_FLAG_MODULE2_ADDR            0x88E0
#define GC08A3_FLAG_LSC1_ADDR               0x1720
#define GC08A3_FLAG_LSC2_ADDR               0x8A60
#define GC08A3_FLAG_AWB1_ADDR               GC08A3_FLAG_MODULE1_ADDR
#define GC08A3_FLAG_AWB2_ADDR               GC08A3_FLAG_MODULE2_ADDR
#define GC08A3_FLAG_AF1_ADDR                0x16F0
#define GC08A3_FLAG_AF2_ADDR                0x8A30

#define GC08A3_CHECKSUM_LEN                1

#define GC08A3_DATA_LEN_MODULE             (0x28+GC08A3_CHECKSUM_LEN)
#define GC08A3_DATA_LEN_MODULE_VALID       (0x1F+GC08A3_CHECKSUM_LEN)
#define GC08A3_DATA_ADDR_MODULE1           0x15A8
#define GC08A3_DATA_ADDR_MODULE2           0x5240
//#define GC08A3_DATA_ADDR_MODULE3           0x8ED8

#define GC08A3_DATA_LEN_LSC                (0x74C+GC08A3_CHECKSUM_LEN)
#define GC08A3_DATA_LEN_LSC_VALID          (GC08A3_DATA_LEN_LSC-3)
#define GC08A3_DATA_ADDR_LSC1              0x1728
#define GC08A3_DATA_ADDR_LSC2              0x8A68
//#define GC08A3_DATA_ADDR_LSC3              0x90B8

#define GC08A3_DATA_LEN_AWB                (0x13+GC08A3_CHECKSUM_LEN)
#define GC08A3_DATA_LEN_AWB_VALID          (GC08A3_DATA_LEN_AWB-3)
#define GC08A3_DATA_ADDR_AWB1              0x16E8
#define GC08A3_DATA_ADDR_AWB2              0x5380
//#define GC08A3_DATA_ADDR_AWB3              0x9018

#define GC08A3_DATA_LEN_AF                 (0x4+GC08A3_CHECKSUM_LEN)
#define GC08A3_DATA_ADDR_AF1               0x16F8
#define GC08A3_DATA_ADDR_AF2               0x8A38
//#define GC08A3_DATA_ADDR_AF3               0xCB38

#define GC08A3_DATA_ADDR_REG_H             0x0A69
#define GC08A3_DATA_ADDR_REG_L             0x0A6A
#define GC08A3_DATA_READ_FLAG_REG          0x0313
#define GC08A3_DATA_READ_REG               0x0A6C

// GC08A3_OTP_SIZE = 0x791
#define GC08A3_OTP_SIZE                   (GC08A3_DATA_LEN_MODULE + \
						GC08A3_DATA_LEN_LSC + \
						GC08A3_DATA_LEN_AWB + GC08A3_DATA_LEN_AF)

// GC08A3_OTP_SIZE_VALID = 0x783
#define GC08A3_OTP_SIZE_VALID             (GC08A3_DATA_LEN_MODULE_VALID + \
						GC08A3_DATA_LEN_LSC_VALID + \
						GC08A3_DATA_LEN_AWB_VALID + \
						GC08A3_DATA_LEN_AF)
#endif

