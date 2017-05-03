/*
 *  Copyright (C) 2010, Imagis Technology Co. Ltd. All Rights Reserved.
 * Copyright (C) 2017 XiaoMi, Inc.
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

#ifndef __IST30XXC_UPDATE_H__
#define __IST30XXC_UPDATE_H__


#define IST30XX_FLASH_BASE_ADDR     (0x0)
#if (IMAGIS_TSP_IC < IMAGIS_IST3038C)
#define IST30XX_FLASH_TOTAL_SIZE    (0x8400)
#else
#define IST30XX_FLASH_TOTAL_SIZE    (0xFC00)
#define IST30XX_FLASH_PAGE_SIZE     (0x0400)
#endif


#define IST30XX_FLASH_BASE          (0x40006000)
#define IST30XX_FLASH_MODE          IST30XX_DA_ADDR(IST30XX_FLASH_BASE | 0x00)
#define IST30XX_FLASH_ADDR          IST30XX_DA_ADDR(IST30XX_FLASH_BASE | 0x04)
#define IST30XX_FLASH_DIN           IST30XX_DA_ADDR(IST30XX_FLASH_BASE | 0x08)
#define IST30XX_FLASH_DOUT          IST30XX_DA_ADDR(IST30XX_FLASH_BASE | 0x0C)
#define IST30XX_FLASH_ISPEN         IST30XX_DA_ADDR(IST30XX_FLASH_BASE | 0x10)
#define IST30XX_FLASH_AUTO_READ     IST30XX_DA_ADDR(IST30XX_FLASH_BASE | 0x14)
#define IST30XX_FLASH_CRC           IST30XX_DA_ADDR(IST30XX_FLASH_BASE | 0x18)
#define IST30XX_FLASH_TEST_MODE1    IST30XX_DA_ADDR(IST30XX_FLASH_BASE | 0x30)
#define IST30XX_FLASH_STATUS        IST30XX_DA_ADDR(IST30XX_FLASH_BASE | 0x90)


#define IST30XX_FW_NAME             "ist30xxc.fw"
#define IST30XX_BIN_NAME            "ist30xxc.bin"


#define CALIB_WAIT_TIME             (50)        /* unit : 100msec */
#define CALIB_TO_GAP(n)             ((n >> 16) & 0xFFF)
#define CALIB_TO_STATUS(n)          ((n >> 12) & 0xF)
#define CALIB_TO_OS_VALUE(n)        ((n >> 12) & 0xFFFF)


#define CAL_REF_TO_STATUS(n)        ((n >> 8) & 0xFFFF)
#define CAL_REF_NONE                (1 << 0)
#define CAL_REF_OK                  (1 << 4)
#define CAL_REF_OK_VERIFY           (1 << 5)
#define CAL_REF_FAIL_UNKNOWN        (1 << 8)
#define CAL_REF_FAIL_CRC            (1 << 9)
#define CAL_REF_FAIL_VERIFY         (1 << 10)


#define MASK_UPDATE_INTERNAL        (1)
#define MASK_UPDATE_FW              (2)
#define MASK_UPDATE_SDCARD          (3)
#define MASK_UPDATE_ERASE           (4)


#define FLAG_MAIN                   (1)
#define FLAG_FW                     (2)
#define FLAG_CORE                   (3)
#define FLAG_TEST                   (4)

int ist30xxc_read_chksum(struct ist30xx_data *data, u32 *chksum);
int ist30xxc_read_chksum_all(struct ist30xx_data *data, u32 *chksum);

int ist30xxc_isp_enable(struct ist30xx_data *data, bool enable);
int ist30xxc_isp_info_read(struct ist30xx_data *data, u32 addr, u32 *buf32,
	u32 len);

int ist30xx_get_update_info(struct ist30xx_data *data, const u8 *buf,
	const u32 size);
int ist30xx_get_tsp_info(struct ist30xx_data *data);
void ist30xx_print_info(struct ist30xx_data *data);
u32 ist30xx_parse_ver(struct ist30xx_data *data, int flag, const u8 *buf);

int ist30xx_fw_update(struct ist30xx_data *data, const u8 *buf, int size);
int ist30xx_fw_recovery(struct ist30xx_data *data);

int ist30xx_auto_bin_update(struct ist30xx_data *data);

int ist30xx_calibrate(struct ist30xx_data *data, int wait_cnt);
int ist30xx_cal_reference(struct ist30xx_data *data);

int ist30xx_init_update_sysfs(struct ist30xx_data *data);

#endif
