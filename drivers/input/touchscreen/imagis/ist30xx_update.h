/*
 *  Copyright (C) 2010, Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2015 XiaoMi, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __IST30XX_UPDATE_H__
#define __IST30XX_UPDATE_H__

#include "ist30xx_tsp.h"

#define IST30XX_INTERNAL_BIN    (1)
#define IST30XX_CHECK_CALIB     (0)

#if IST30XX_INTERNAL_BIN
#define IST30XX_UPDATE_BY_WORKQUEUE     (0)
#define IST30XX_UPDATE_DELAY    (3 * HZ)
#endif // IST30XX_INTERNAL_BIN

#if ((IMAGIS_TSP_IC == IMAGIS_IST30XX) || (IMAGIS_TSP_IC == IMAGIS_IST30XXB))
#define IST30XX_EEPROM_SIZE     (0x8000)
#elif (IMAGIS_TSP_IC == IMAGIS_IST3038)
#define IST30XX_EEPROM_SIZE     (0xA000)
#elif (IMAGIS_TSP_IC == IMAGIS_IST3044)
#define IST30XX_EEPROM_SIZE     (0x10000)
#endif
#define EEPROM_PAGE_SIZE        (64)
#define EEPROM_BASE_ADDR        (0)

#define IST30XXB_ACCESS_ADDR        (1 << 31)
#define IST30XXB_BURST_ACCESS       (1 << 27)
#define IST30XXB_DA_ADDR(n)         (n | IST30XXB_ACCESS_ADDR)

#define IST30XXB_REG_TSPTYPE        IST30XXB_DA_ADDR(0x40002010)

#define IST30XXB_REG_EEPMODE        IST30XXB_DA_ADDR(0x40007000)
#define IST30XXB_REG_EEPADDR        IST30XXB_DA_ADDR(0x40007004)
#define IST30XXB_REG_EEPWDAT        IST30XXB_DA_ADDR(0x40007008)
#define IST30XXB_REG_EEPRDAT        IST30XXB_DA_ADDR(0x4000700C)
#define IST30XXB_REG_EEPISPEN       IST30XXB_DA_ADDR(0x40007010)
#define IST30XXB_REG_CHKSMOD        IST30XXB_DA_ADDR(0x40007014)
#define IST30XXB_REG_EEPPWRCTRL     IST30XXB_DA_ADDR(0x4000701C)
#define IST30XXB_REG_CHKSDAT        IST30XXB_DA_ADDR(0x40007038)

#define IST30XXB_REG_CHIPID         IST30XXB_DA_ADDR(0x40000000)
#define IST30XXB_REG_LDOOSC         IST30XXB_DA_ADDR(0x40000030)
#define IST30XXB_REG_CLKDIV         IST30XXB_DA_ADDR(0x4000004C)

#define IST30XXB_MEM_ADDR           (0x20000000)
#define IST30XXB_MEM_ALGORITHM      IST30XXB_DA_ADDR(IST30XXB_MEM_ADDR | 0x34)
#define IST30XXB_MEM_CMD            IST30XXB_DA_ADDR(IST30XXB_MEM_ADDR | 0x3C)
#define IST30XXB_MEM_FINGERS        IST30XXB_DA_ADDR(IST30XXB_MEM_ADDR | 0x4)
#define IST30XXB_MEM_COMMAND      	IST30XXB_DA_ADDR(IST30XXB_MEM_ADDR | 0x30)

#if (IMAGIS_TSP_IC == IMAGIS_IST30XXB)
#define IST30XXB_MEM_COUNT          IST30XXB_DA_ADDR(IST30XXB_MEM_ADDR | 0x5F0)
#elif (IMAGIS_TSP_IC == IMAGIS_IST3038)
#define IST30XXB_MEM_COUNT          IST30XXB_DA_ADDR(IST30XXB_MEM_ADDR | 0x40)
#else
#error "Unknown IMAGIS_TSP_IC"
#endif

#define IST30XX_FW_NAME         "ist30xx.fw"
#define IST30XXB_FW_NAME        "ist30xxb.fw"
#define IST30XXB_CURVE_NAME     "ist30xxb.cv"

#define MASK_FW_VER             (0xFFFF0000)
#define IST30XX_FW_VER1         (0x00010000)
#define IST30XX_FW_VER2         (0x00020000)
#define IST30XX_FW_VER3         (0x00030000)
#define IST30XX_FW_VER4         (0x00040000)

#define IST30XX_FW_UPDATE_RETRY (3)

#define WAIT_CALIB_CNT          (50)
#define CALIB_THRESHOLD         (100)

#define CALIB_TO_GAP(n)         ((n >> 16) & 0xFFF)
#define CALIB_TO_STATUS(n)      ((n >> 12) & 0xF)

#define CALIB_TO_OS_VALUE(n)    ((n >> 12) & 0xFFFF)

#define MASK_UPDATE_BIN         (1)
#define MASK_UPDATE_FW          (1)
#define MASK_UPDATE_ISP         (2)
#define MASK_UPDATE_ALL         (3)

#define CHKSUM_FW               (1)
#define CHKSUM_ALL              (2)

#define FLAG_FW                 (1)
#define FLAG_PARAM              (2)
#define FLAG_SUB                (3)

#define TAGS_PARSE_OK           (0)

/* I2C Transaction size */
#define I2C_MAX_WRITE_SIZE      (64)            /* bytes */
#define I2C_MAX_READ_SIZE       (8)             /* bytes */
#define I2C_DA_MAX_READ_SIZE    (4)             /* bytes */

int ist30xx_read_buf(struct i2c_client *client, u32 cmd, u32 *buf, u16 len);
int ist30xx_write_buf(struct i2c_client *client, u32 cmd, u32 *buf, u16 len);

int ist30xxb_burst_read(struct i2c_client *client, u32 addr, u32 *buf32, int len);

int ist30xxb_read_chksum(struct i2c_client *client, u32 *chksum);

void ist30xx_get_update_info(struct ist30xx_data *data, const u8 *buf, const u32 size);

u32 ist30xx_parse_ver(struct ist30xx_data *data, int flag, const u8 *buf);

int ist30xx_fw_update(struct i2c_client *client, const u8 *buf, int size, bool mode);
int ist30xx_fw_param_update(struct i2c_client *client, const u8 *buf);
int ist30xx_fw_recovery(struct ist30xx_data *data);

int ist30xx_auto_fw_update(struct ist30xx_data *data);
int ist30xx_auto_param_update(struct ist30xx_data *data);
int ist30xx_auto_bin_update(struct ist30xx_data *data);

int ist30xx_calibrate(struct ist30xx_data *data, int wait_cnt);
int ist30xx_calib_wait(struct ist30xx_data *data);

int ist30xx_init_update_sysfs(struct ist30xx_data *data);

#endif  // __IST30XX_UPDATE_H__
