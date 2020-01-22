/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016-2019, STMicroelectronics Limited.
 * Authors: AMG(Analog Mems Group) <marco.cali@st.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 *
 **************************************************************************
 **                        STMicroelectronics                            **
 **************************************************************************
 **                        marco.cali@st.com                             **
 **************************************************************************
 *                                                                        *
 *                        FTS API for Flashing the IC                     *
 *                                                                        *
 **************************************************************************
 **************************************************************************
 *
 */

#ifndef __FTS_FLASH_H
#define __FTS_FLASH_H


#include "ftsSoftware.h"

//Flash possible status
#define FLASH_READY              0
#define FLASH_BUSY               1
#define FLASH_UNKNOWN           -1
#define FLASH_STATUS_BYTES       1


//Flash timing parameters
#define FLASH_RETRY_COUNT        1000
#define FLASH_WAIT_BEFORE_RETRY  50   //ms
#define FLASH_WAIT_TIME          200  //ms


//PATHS FW FILES
//#define PATH_FILE_FW           "fw.memh"
#ifdef FTM3_CHIP
#define PATH_FILE_FW             "st_fts.bin"
#else
#define PATH_FILE_FW             "st_fts.ftb"//new bin file structure
#endif

#ifndef FTM3_CHIP
#define FLASH_CHUNK              (64 * 1024)
#define DMA_CHUNK                (2 * 1024)
#endif


struct Firmware {
	u8 *data;
	u16 fw_ver;
	u16 config_id;
	u8 externalRelease[EXTERNAL_RELEASE_INFO_SIZE];
	int data_size;
#ifndef FTM3_CHIP
	u32 sec0_size;
	u32 sec1_size;
	u32 sec2_size;
	u32 sec3_size;
#endif
};

#ifdef FTM3_CHIP
int flash_status(void);
int flash_status_ready(void);
int wait_for_flash_ready(void);
#else
int wait_for_flash_ready(u8 type);
int fts_warm_boot(void);
int flash_erase_unlock(void);
int flash_full_erase(void);
int flash_erase_page_by_page(int keep_cx);
//int flash_erase_page_by_page_info(int page);
int start_flash_dma(void);
int fillFlash(u32 address, u8 *data, int size);
#endif

int flash_unlock(void);
int fillMemory(u32 address, u8 *data, int size);
int getFirmwareVersion(u16 *fw_vers, u16 *config_id);
int getFWdata(const char *pathToFile, u8 **data, int *size, int from);
int getFWdata_nocheck(const char *pathToFile, u8 **data, int *size, int from);
int parseBinFile(u8 *fw_data, int fw_size, struct Firmware *fw, int keep_cx);
int readFwFile(const char *path, struct Firmware *fw, int keep_cx);
int flash_burn(struct Firmware *fw, int force_burn, int keep_cx);
int flashProcedure(const char *path, int force, int keep_cx);

#endif
