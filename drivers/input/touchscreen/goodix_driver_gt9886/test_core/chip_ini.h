/******************** (C) COPYRIGHT 2018 Goodix ********************
* File Name          : chip_ini.h
* Author             : zhitao yang
* Version            : V1.0.0
* Date               : 14/11/2018
* Description        : chip info
*******************************************************************************/
#ifndef CHIP_INI_H
#define CHIP_INI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tp_product_id_def.h"
#include "user_test_type_def.h"

/*cfg*/
#define  CFG_START_ADDR					0x6F78
#define  CFG_LEN						690

#define  SEN_MAX_NUM_IN_DIE				36
#define  DRV_MAX_NUM_IN_DIE				40
#define  MAX_SEN_NUM					36
#define  MAX_DRV_NUM					40
#define  MAX_KEY_NUM					4

/*cmd*/
#define  CMD_ADDR						0x6F68

/*rawdata*/
#define  DATA_ENDIAN					1
#define  DATA_INVERT					1
#define  RAWDATA_ADDR					0x8FA0
#define  RAW_UNIT_OF_BYTE				2
#define  RAW_SIGNED						1
#define  SYN_ADDR						0x4100
#define  SYN_MASK						0x80

#define  MAP_DISABLE					0

u8 IC_NAME[] = "GT9886";
u8 SEN_NUM_ADDR[] = "T1,10,0,7";
u8 DRV_NUM_ADDR[] = "T1,14,0,7,T1,15,0,7";

u8 SEN_START_ADDR[] = "T2,2,0,7";
u8 DRV_START_ADDR[] = "T2,38,0,7";

u8 KEY_START_ADDR[] = "T16,12,0,15";
u8 SEN_AS_KEY_ADDR[] = "T16,2,3,3";
u8 KEY_PORT_NUM[] = "";
u8 KEY_EN[] = "T16,2,0,0";
u8 HOPPING_EN[] = "T10,2,0,0";

/*channel map*/
u8 DRV_MAP[] = {46, 48, 49, 47, 45, 50, 56, 52, 51, 53, 55, 54, 59, 64, 57, 60, 62, 58,
	65, 63, 61, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255};
u8 SEN_MAP[] = {32, 34, 35, 30, 31, 33, 27, 28, 29, 10, 25, 26, 23, 13, 24, 12, 9, 11,
	8, 7, 5, 6, 4, 3, 2, 1, 0, 73, 75, 74, 39, 72, 40, 36, 37, 38};
/*cmd buf*/
u8 RAW_CMD_BUF[] = { 0x01, 0x00, 0xff };
u8 COOR_CMD_BUF[] = { 0x00, 0x00, 0x00 };

#ifdef __cplusplus
}
#endif
#endif
