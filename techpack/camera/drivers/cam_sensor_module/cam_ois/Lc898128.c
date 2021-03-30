// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <cam_sensor_cmn_header.h>
#include "cam_ois_core.h"
#include "cam_ois_soc.h"
#include "cam_sensor_util.h"
#include "cam_debug_util.h"
#include "cam_res_mgr_api.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"
#include "Lc898128.h"
#include <linux/vmalloc.h>

const uint8_t PACT1Tbl[] = { 0x20, 0xDF };	/* [ACT_02][ACT_01][ACT_03] */

void RamWrite32A(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t data, uint32_t delay)
{
	struct cam_sensor_i2c_reg_setting i2c_reg_settings = {0};
	struct cam_sensor_i2c_reg_array i2c_reg_array = {0};
	int32_t rc = 0;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return;
	}

	i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_settings.data_type = CAMERA_SENSOR_I2C_TYPE_DWORD;
	i2c_reg_settings.size = 1;
	i2c_reg_array.reg_addr = addr;
	i2c_reg_array.reg_data = data;
	i2c_reg_array.delay = delay;
	i2c_reg_settings.reg_setting = &i2c_reg_array;

	rc = camera_io_dev_write(&o_ctrl->io_master_info,
		&i2c_reg_settings);
	if (rc) {
		CAM_ERR(CAM_OIS, "%s : write addr 0x%04x data 0x%x failed rc %d",
			o_ctrl->ois_name, addr, data, rc);
	}
	CAM_DBG(CAM_OIS, "[OISDEBUG] write addr 0x%08x = 0x%08x", addr, data);
}

void IOWrite32A(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t data, uint32_t delay)
{
	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return;
	}

	RamWrite32A(o_ctrl, CMD_IO_ADR_ACCESS, addr, 0);
	RamWrite32A(o_ctrl, CMD_IO_DAT_ACCESS, data, delay);
//	CAM_DBG(CAM_OIS, "OIS %s :  write addr 0x%08x = 0x%08x", o_ctrl->ois_name, addr, data);
}

void RamRead32A(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t* data)
{
	uint32_t read_val = 0;
	int32_t rc = 0;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return;
	}

	rc = camera_io_dev_read(&(o_ctrl->io_master_info),
		addr, &read_val,
		CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_DWORD);
	if (!rc && data) {
		*data = read_val;
	} else {
		CAM_ERR(CAM_OIS, "%s : read fail at addr 0x%x, data = %p, rc = %d",
			o_ctrl->ois_name, addr, data, rc);
	}
	CAM_DBG(CAM_OIS, "[OISDEBUG] read addr 0x%04x = 0x%08x", addr, *data);
}

void IORead32A(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t* data)
{
	uint32_t read_val = 0;
	int32_t rc = 0;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return;
	}

	RamWrite32A(o_ctrl, CMD_IO_ADR_ACCESS, addr, 0);
	rc = camera_io_dev_read(&(o_ctrl->io_master_info),
		CMD_IO_DAT_ACCESS, &read_val,
		CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_DWORD);
	if (!rc && data) {
		*data = read_val;
	} else {
		CAM_ERR(CAM_OIS, "%s : read fail at addr 0x%x, data = %p, rc = %d",
			o_ctrl->ois_name, addr, data, rc);
	}
	CAM_DBG(CAM_OIS, "[OISDEBUG] read addr 0x%04x = 0x%08x", addr, *data);
}

void CntWrt(struct cam_ois_ctrl_t *o_ctrl, uint8_t *data, uint32_t length, uint32_t delay)
{
	static struct cam_sensor_i2c_reg_array w_data[256] = { {0} };
	struct cam_sensor_i2c_reg_setting write_setting;
	uint32_t i = 0;
	uint32_t addr = 0;
	int32_t rc = 0;
	if (!data || !o_ctrl || (length < 3)) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return;
	}

	addr = (((uint16_t)(data[0]) << 8) | data[1]) & 0xFFFF;
	for (i = 0;i < (length -2) && i < 256; i++) {
		w_data[i].reg_addr = addr;
		w_data[i].reg_data = data[i+2];
		w_data[i].delay = 0;
		w_data[i].data_mask = 0;
	}
	write_setting.size = length - 2;
	write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.delay = delay;
	write_setting.reg_setting = w_data;

	rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
		&write_setting, 1);
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "OIS CntWrt write failed %d", rc);
	}
	for (i = 0; i < (length -2) && i < 256; i+=4) {
		CAM_DBG(CAM_OIS, "[OISDEBUG] write addr 0x%04x = 0x%02x 0x%02x 0x%02x 0x%02x", addr, data[i+2], data[i+3], data[i+4], data[i+5]);
	}
}

void CntRd(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint8_t *data, uint32_t length)
{
	int32_t rc = 0;
	int32_t i = 0;
	rc = camera_io_dev_read_seq(&o_ctrl->io_master_info,
		addr, data,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_BYTE,
		length);
	for (i = 0; i < length; i++) {
		CAM_DBG(CAM_OIS, "[OISDEBUG] read addr 0x%04x[%d] = 0x%02x", addr, i, data[i]);
	}
	if (rc) {
		CAM_ERR(CAM_EEPROM, "read failed rc %d",
			rc);
	}
}

//********************************************************************************
// Function Name 	: FlashBlockErase
// Retun Value		: 0 : Success, 1 : Unlock Error, 2 : Time Out Error
// Argment Value	: Use Mat , Flash Address
// Explanation		: <Flash Memory> Block Erase
// History		: First edition
// Unit of erase	: informaton mat  128 Byte
//			: user mat         4k Byte
//********************************************************************************
uint8_t FlashBlockErase(struct cam_ois_ctrl_t *o_ctrl, uint8_t SelMat , uint32_t SetAddress)
{
	uint32_t UlReadVal, UlCnt;
	uint8_t ans = 0;

	// fail safe
	// reject irregular mat
	if (SelMat != USER_MAT && SelMat != INF_MAT0 && SelMat != INF_MAT1 && SelMat != INF_MAT2)
		return 10;	// INF_MAT2もAccessしない
	// reject command if address inner NVR3
	if(SetAddress > 0x000003FF)
		return 9;

	// Flash write準備
	ans = UnlockCodeSet(o_ctrl);
	if (ans != 0)
		return ans;	// Unlock Code Set

	WritePermission(o_ctrl);	// Write permission
	if (SelMat != USER_MAT) {
		if (SelMat == INF_MAT2)
			IOWrite32A(o_ctrl, 0xE07CCC, 0x00006A4B, 0);
		else
			IOWrite32A(o_ctrl, 0xE07CCC, 0x0000C5AD, 0);	// additional unlock for INFO
	}
	AddtionalUnlockCodeSet(o_ctrl);	// common additional unlock code set

	IOWrite32A(o_ctrl, FLASHROM_FLA_ADR, ((uint32_t)SelMat << 16) | ( SetAddress & 0x00003C00 ), 0);
	// Sector Erase Start
	IOWrite32A(o_ctrl, FLASHROM_FLAMODE, 0x00000000, 0);
	IOWrite32A(o_ctrl, FLASHROM_CMD, 4, 0) ;

	msleep(5);

	UlCnt = 0;
	do {
		if (UlCnt++ > 100) {
			ans = 2;
			break;
		}
		IORead32A(o_ctrl, FLASHROM_FLAINT, &UlReadVal);
	} while (( UlReadVal & 0x00000080 ) != 0);

	IOWrite32A(o_ctrl, FLASHROM_FLAMODE , 0x00000002, 0);
	ans = UnlockCodeClear(o_ctrl);	// Unlock Code Clear
	if (ans != 0)
		return ans;	// Unlock Code Set

	return ans;
}

//********************************************************************************
// Function Name 	: FlashSingleRead
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: <Flash Memory> Flash Single Read( 4 Byte read )
// History			: First edition
//********************************************************************************
uint8_t FlashSingleRead(struct cam_ois_ctrl_t *o_ctrl, uint8_t SelMat, uint32_t UlAddress, uint32_t *PulData)
{
	// fail safe
	// reject irregular mat
	if( SelMat != USER_MAT && SelMat != INF_MAT0 && SelMat != INF_MAT1 && SelMat != INF_MAT2  )
		return 10;	// INF_MAT2もAccessしない
	// reject command if address inner NVR3
	if( UlAddress > 0x000003FF )											return 9;

	IOWrite32A(o_ctrl, FLASHROM_ACSCNT, 0x00000000, 0);
	IOWrite32A(o_ctrl, FLASHROM_FLA_ADR, ((uint32_t)SelMat << 16) | ( UlAddress & 0x00003FFF ), 0);

	IOWrite32A(o_ctrl, FLASHROM_FLAMODE, 0x00000000, 0);
	IOWrite32A(o_ctrl, FLASHROM_CMD, 0x00000001, 0);

	IORead32A(o_ctrl, FLASHROM_FLA_RDAT, PulData) ;
	IOWrite32A(o_ctrl, FLASHROM_FLAMODE , 0x00000002, 0);

	return 0;
}

//********************************************************************************
// Function Name 	: FlashMultiRead
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: <Flash Memory> Flash Multi Read( 4 Byte * length  max read : 128byte)
// History			: First edition
//********************************************************************************
uint8_t FlashMultiRead(struct cam_ois_ctrl_t *o_ctrl, uint8_t SelMat, uint32_t UlAddress, uint32_t *PulData , uint8_t UcLength)
{
	uint8_t i;

	// fail safe
	// reject irregular mat
	if( SelMat != USER_MAT && SelMat != INF_MAT0 && SelMat != INF_MAT1 && SelMat != INF_MAT2  )
		return 10;	// INF_MAT2はRead only Accessしない
	// reject command if address inner NVR3
	if( UlAddress > 0x000003FF )
		return 9;

	IOWrite32A(o_ctrl, FLASHROM_ACSCNT, 0x00000000 | (uint32_t)(UcLength-1), 0);
	IOWrite32A(o_ctrl, FLASHROM_FLA_ADR, ((uint32_t)SelMat << 16) | ( UlAddress & 0x00003FFF ), 0);

	IOWrite32A(o_ctrl, FLASHROM_FLAMODE , 0x00000000, 0);
	IOWrite32A(o_ctrl, FLASHROM_CMD, 0x00000001, 0);
	for( i=0 ; i < UcLength ; i++ ){
		IORead32A(o_ctrl, FLASHROM_FLA_RDAT, &PulData[i]);
		CAM_DBG(CAM_OIS, "Read Data[%d] = 0x%08x", i, PulData[i]);
	}
	IOWrite32A(o_ctrl, FLASHROM_FLAMODE, 0x00000002, 0);

	return 0;
}

//********************************************************************************
// Function Name 	: FlashBlockWrite
// Retun Value		: 0 : Success, 1 : Unlock Error, 2 : Time Out Error
// Argment Value	: Info Mat , Flash Address
// Explanation		: <Flash Memory> Block Erase
// History		: First edition
// Unit of erase	: informaton mat   64 Byte
//					: user mat         64 Byte
//********************************************************************************
uint8_t FlashBlockWrite(struct cam_ois_ctrl_t *o_ctrl, uint8_t SelMat, uint32_t SetAddress, uint32_t *PulData)
{
	uint32_t UlReadVal, UlCnt;
	uint8_t ans = 0;
	uint8_t i;

	// fail safe
	// reject irregular mat
//	if( SelMat != INF_MAT0 && SelMat != INF_MAT1  )
//		return 10;	// USR MAT,INF_MAT2もAccessしない
	if (SelMat != INF_MAT0 && SelMat != INF_MAT1 && SelMat != INF_MAT2)
		return 10;	// USR MAT

	if (SetAddress > 0x000003FF)
		return 9;

	// Flash write
	ans = UnlockCodeSet(o_ctrl);
	if (ans != 0)
		return ans;	// Unlock Code Set

	WritePermission(o_ctrl);	// Write permission
	if (SelMat != USER_MAT) {
		if(SelMat == INF_MAT2)
			IOWrite32A(o_ctrl, 0xE07CCC, 0x00006A4B, 0);
		else
			IOWrite32A(o_ctrl, 0xE07CCC, 0x0000C5AD, 0);	// additional unlock for INFO
	}
	AddtionalUnlockCodeSet(o_ctrl);	// common additional unlock code set

	IOWrite32A(o_ctrl, FLASHROM_FLA_ADR, ((uint32_t)SelMat << 16) | ( SetAddress & 0x000010), 0);
	// page write Start
	IOWrite32A(o_ctrl, FLASHROM_FLAMODE , 0x00000000, 0);
	IOWrite32A(o_ctrl, FLASHROM_CMD, 2, 0);

//	msleep(5);

	UlCnt = 0;

	for (i = 0; i < 16; i++) {
		IOWrite32A(o_ctrl, FLASHROM_FLA_WDAT, PulData[i], 0);	// Write data
		CAM_DBG(CAM_OIS, "Write Data[%d] = %08x", i , PulData[i] );
	}
	do {
		if (UlCnt++ > 100) {
			ans = 2;
			break;
		}
		IORead32A(o_ctrl, FLASHROM_FLAINT, &UlReadVal);
	} while(( UlReadVal & 0x00000080 ) != 0);

	CAM_DBG(CAM_OIS, "[FlashBlockWrite] BEGIN FLASHROM_CMD Write Data");
	// page program
	IOWrite32A(o_ctrl, FLASHROM_CMD, 8, 0);

        CAM_DBG(CAM_OIS, "[FlashBlockWrite] END FLASHROM_CMD Write Data");
	do {
		if ( UlCnt++ > 100 ) {
			ans = 2;
			break;
		}
		IORead32A(o_ctrl, FLASHROM_FLAINT, &UlReadVal);
	} while (( UlReadVal & 0x00000080 ) != 0);

	IOWrite32A(o_ctrl, FLASHROM_FLAMODE , 0x00000002, 0);
	ans = UnlockCodeClear(o_ctrl);	// Unlock Code Clear
	CAM_DBG(CAM_OIS, "[FlashBlockWrite] END OF FlashBlockWrite ans = %d", ans);
	return ans;
}

//********************************************************************************
// Function Name 	: UnlockCodeSet
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: <Flash Memory> Unlock Code Set
// History			: First edition
//********************************************************************************
uint8_t UnlockCodeSet(struct cam_ois_ctrl_t *o_ctrl)
{
	uint32_t UlReadVal, UlCnt=0;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -1;
	}

	do {
		IOWrite32A(o_ctrl, 0xE07554, 0xAAAAAAAA, 0);	// UNLK_CODE1(E0_7554h) = AAAA_AAAAh
		IOWrite32A(o_ctrl, 0xE07AA8, 0x55555555, 0);	// UNLK_CODE2(E0_7AA8h) = 5555_5555h
		IORead32A(o_ctrl, 0xE07014, &UlReadVal);
		if( (UlReadVal & 0x00000080) != 0 )
			return 0;	// Check UNLOCK(E0_7014h[7]) ?
		msleep(1);
	} while ( UlCnt++ < 10 );
	return 1;
}

//********************************************************************************
// Function Name 	: WritePermission
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: LC898128 Command
// History			: First edition 						2018.05.15
//********************************************************************************
void WritePermission(struct cam_ois_ctrl_t *o_ctrl)
{
	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return;
	}

	IOWrite32A(o_ctrl, 0xE074CC, 0x00000001, 0);	// RSTB_FLA_WR(E0_74CCh[0])=1
	IOWrite32A(o_ctrl, 0xE07664, 0x00000010, 0);	// FLA_WR_ON(E0_7664h[4])=1
}

//********************************************************************************
// Function Name 	: AddtionalUnlockCodeSet
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: LC898128 Command
// History			: First edition 						2018.05.15
//********************************************************************************
void AddtionalUnlockCodeSet(struct cam_ois_ctrl_t *o_ctrl)
{
	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return;
	}
	IOWrite32A(o_ctrl, 0xE07CCC, 0x0000ACD5, 0);// UNLK_CODE3(E0_7CCCh) = 0000_ACD5h
}

//********************************************************************************
// Function Name 	: EraseUserMat128
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: User Mat All Erase

// History			: First edition
//********************************************************************************
uint8_t EraseUserMat128(struct cam_ois_ctrl_t *o_ctrl, uint8_t StartBlock, uint8_t EndBlock)
{
	uint32_t i;
	uint32_t UlReadVal, UlCnt;

	IOWrite32A(o_ctrl, 0xE0701C, 0x00000000, 0);
	RamWrite32A(o_ctrl, 0xF007, 0x00000000, 0);	// FlashAccess Setup

	//***** User Mat *****
	for(i = StartBlock; i<EndBlock ; i++) {
		RamWrite32A(o_ctrl, 0xF00A, ( i << 10 ), 0);	// FromCmd.Addrの設定
		RamWrite32A(o_ctrl, 0xF00C, 0x00000020, 0);	// FromCmd.Controlの設定(ブロック消去)

		msleep(5);
		UlCnt = 0;
		do {
			msleep(1);
			if( UlCnt++ > 10 ){
				IOWrite32A(o_ctrl, 0xE0701C, 0x00000002, 0);
				return (0x31) ;	// block erase timeout ng
			}
			RamRead32A(o_ctrl, 0xF00C, &UlReadVal);	// FromCmd.Control
		} while ( UlReadVal != 0 );
	}
	IOWrite32A(o_ctrl, 0xE0701C ,0x00000002, 0);
	return 0;

}

//********************************************************************************
// Function Name 	: UnlockCodeClear
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: <Flash Memory> Clear Unlock Code
// History			: First edition
//********************************************************************************
uint8_t UnlockCodeClear(struct cam_ois_ctrl_t *o_ctrl)
{
	uint32_t UlDataVal, UlCnt=0;

	do {
		IOWrite32A(o_ctrl, 0xE07014, 0x00000010, 0);	// UNLK_CODE3(E0_7014h[4]) = 1
		IORead32A(o_ctrl, 0xE07014, &UlDataVal);
		if( (UlDataVal & 0x00000080) == 0 )
			return 0;	// Check UNLOCK(E0_7014h[7]) ?
		msleep(1);
	} while( UlCnt++ < 10 );
	return 3;
}

uint32_t CheckFwValid(struct cam_ois_ctrl_t *o_ctrl, uint8_t CurrentFwVersion)
{
	uint32_t fw_status = 0xFF;
	uint32_t is_fw_valid = 0;
	uint32_t fw_version = 0;
	uint8_t i;

	CAM_DBG(CAM_OIS, "checking fw");

	for (i = 0; i < 40; i++) {
		IORead32A(o_ctrl, SYSDSP_REMAP, &fw_status);
		if (fw_status == 0x1) {
			is_fw_valid = 1;
			break;
		}
		msleep(1);
	}

	if (is_fw_valid) {
		RamRead32A(o_ctrl, SiVerNum, &fw_version);
		fw_version &= 0xFF;
		if (fw_version != CurrentFwVersion){
			is_fw_valid = 0;
		}
	}

	CAM_DBG(CAM_OIS, "is_fw_valid %d, fw_version 0x%x", is_fw_valid, fw_version);

	return is_fw_valid;
}

//********************************************************************************
// Function Name 	: CheckDrvOffAdj
// Retun Value		: Driver Offset Re-adjustment
// Argment Value	: NON
// Explanation		: Driver Offset Re-adjustment
// History		: First edition
//********************************************************************************
uint32_t CheckDrvOffAdj(struct cam_ois_ctrl_t *o_ctrl)
{
	uint32_t UlReadDrvOffx, UlReadDrvOffy, UlReadDrvOffaf;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}

	IOWrite32A(o_ctrl, FLASHROM_ACSCNT, 2, 0); //3word
	IOWrite32A(o_ctrl, FLASHROM_FLA_ADR, ((uint32_t)INF_MAT1 << 16) | 0xD, 0);
	IOWrite32A(o_ctrl, FLASHROM_FLAMODE, 0x00000000, 0);
	IOWrite32A(o_ctrl, FLASHROM_CMD, 0x00000001, 0);

	IORead32A(o_ctrl, FLASHROM_FLA_RDAT, &UlReadDrvOffaf); // #13
	IORead32A(o_ctrl, FLASHROM_FLA_RDAT, &UlReadDrvOffx); // #14
	IORead32A(o_ctrl, FLASHROM_FLA_RDAT, &UlReadDrvOffy); // #15

	IOWrite32A(o_ctrl, FLASHROM_FLAMODE , 0x00000002, 0);
	if(((UlReadDrvOffx & 0x000FF00) == 0x0000100) ||
		((UlReadDrvOffy & 0x000FF00) == 0x0000100) ||
		((UlReadDrvOffaf & 0x000FF00) == 0x0000800)) {		//error
		return( 0x93 );
	}

	if( ((UlReadDrvOffx & 0x0000080) == 0) &&
		((UlReadDrvOffy & 0x00000080) ==0) &&
		((UlReadDrvOffaf & 0x00008000) ==0)) {
		return( 0 ); 	// 0 : Uppdated
	}

	return( 1 );		// 1 : Still not.
}

uint32_t DrvOffAdj(struct cam_ois_ctrl_t *o_ctrl)
{
	uint8_t ans = 0;
	uint32_t UlReadVal;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}

	ans = CheckDrvOffAdj(o_ctrl);
	CAM_DBG(CAM_OIS, "[OISDEBUG] CheckDrvOffAdj result %d", ans);
	if (ans == 1) {
		CAM_DBG(CAM_OIS, "[OISDEBUG] ready to update prog fw");
	 	ans = CoreResetwithoutMC128(o_ctrl); // Start up to boot exection
	 	if(ans != 0)
			return( ans );
	 	ans = PmemUpdate128(o_ctrl, 0);
		if(ans != 0)
			return( ans );

		IOWrite32A(o_ctrl, FLASHROM_FLAMODE, 0x00000000, 0);
		RamWrite32A(o_ctrl, 0xF001,  0x00000000, 1000);
		IOWrite32A(o_ctrl, 0xE07CCC, 0x0000C5AD, 0); // additional unlock for INFO
		IOWrite32A(o_ctrl, 0xE07CCC, 0x0000ACD5, 10000); // UNLK_CODE3(E0_7CCCh) = 0000_ACD5h

		IOWrite32A(o_ctrl, FLASHROM_FLAMODE , 0x00000002, 0);
		IOWrite32A(o_ctrl, SYSDSP_REMAP, 0x00001000, 15000);// CORE_RST[12], MC_IGNORE2[10] = 1 PRAMSEL[7:6]=01b
		IORead32A(o_ctrl, ROMINFO, &UlReadVal);
		if(UlReadVal != 0x08)
			return 0x90;

		ans = CheckDrvOffAdj(o_ctrl);
	}
	return ans;
}

//********************************************************************************
// Function Name 	: Mat2ReWrite
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Mat2 re-write function
// History		: First edition
//********************************************************************************
uint8_t Mat2ReWrite(struct cam_ois_ctrl_t *o_ctrl)
{
	uint32_t UlMAT2[32];
	uint32_t UlCKSUM=0;
	uint32_t UlCkVal, UlCkVal_Bk;
	uint8_t ans, i;

	ans = FlashMultiRead(o_ctrl, INF_MAT2, 0, UlMAT2, 32);
	if(ans)	return( 0xA0 );
	/* FT_REPRG check *****/
//	if( UlMAT2[FT_REPRG] == PRDCT_WR || UlMAT2[FT_REPRG] == USER_WR ){
	if( UlMAT2[FT_REPRG] == USER_WR ){
		return( 0x00 );	// not need
	}

	/* Check code check *****/
	if( UlMAT2[CHECKCODE1] != CHECK_CODE1 )	return( 0xA1 );
	if( UlMAT2[CHECKCODE2] != CHECK_CODE2 )	return( 0xA2 );

	/* Check sum check *****/
	for( i=16 ; i<MAT2_CKSM ; i++){
		UlCKSUM += UlMAT2[i];
	}
	if(UlCKSUM != UlMAT2[MAT2_CKSM])		return( 0xA3 );

	/* registor re-write flag *****/
	UlMAT2[FT_REPRG] = USER_WR;

	/* backup sum check before re-write *****/
	UlCkVal_Bk = 0;
	for( i=0; i < 32; i++ ){		// 全領域
		UlCkVal_Bk +=  UlMAT2[i];
	}

	/* Erase   ******************************************************/
	ans = FlashBlockErase(o_ctrl, INF_MAT2, 0);	// all erase
	if (ans != 0)
		return 0xA4;	// Unlock Code Set

	/* excute re-write *****/
	ans = FlashBlockWrite(o_ctrl, INF_MAT2 , 0, UlMAT2);
	if (ans != 0)
		return 0xA5;	// Unlock Code Set
	ans = FlashBlockWrite(o_ctrl, INF_MAT2, (uint32_t)0x10, &UlMAT2[0x10]);
	if (ans != 0)
		return 0xA5;	// Unlock Code Set

	ans = FlashMultiRead(o_ctrl, INF_MAT2, 0, UlMAT2, 32);
	if (ans)
		return 0xA0;
	UlCkVal = 0;
	for (i = 0; i < 32; i++) {
		UlCkVal +=  UlMAT2[i];
	}

	if (UlCkVal != UlCkVal_Bk)
		return 0xA6;	// write data != writen data
	return 0x01;	// re-write ok
}

uint8_t CoreResetwithoutMC128(struct cam_ois_ctrl_t *o_ctrl)
{
	uint32_t UlReadVal = 0;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}

	IOWrite32A(o_ctrl, 0xE07554, 0xAAAAAAAA, 0);
	IOWrite32A(o_ctrl, 0xE07AA8, 0x55555555, 0);

	IOWrite32A(o_ctrl, 0xE074CC, 0x00000001, 0);
	IOWrite32A(o_ctrl, 0xE07664, 0x00000010, 0);
	IOWrite32A(o_ctrl, 0xE07CCC, 0x0000ACD5, 0);
	IOWrite32A(o_ctrl, 0xE0700C, 0x00000000, 0);
	IOWrite32A(o_ctrl, 0xE0701C, 0x00000000, 0);
	IOWrite32A(o_ctrl, 0xE07010, 0x00000004, 100000);

	IOWrite32A(o_ctrl, 0xE0701C, 0x00000002, 0);
	IOWrite32A(o_ctrl, 0xE07014, 0x00000010, 0);

	IOWrite32A(o_ctrl, 0xD00060, 0x00000001, 15000) ;

	IORead32A(o_ctrl, ROMINFO, &UlReadVal);

	CAM_DBG(CAM_OIS, "[OISDEBUG] read from addr 0x%04x =  0x%x", ROMINFO, UlReadVal);
	switch ((uint8_t)UlReadVal) {
	case 0x08:
	case 0x0D:
		break;
	default:
		return(0xE0 | (uint8_t)UlReadVal);
	}

	return 0;
}

//********************************************************************************
// Function Name 	: PmemUpdate128
// Retun Value		: 0:Non error
// Argment Value	: None
// Explanation		: Program code Update to PMEM directly
// History			: First edition
//********************************************************************************
uint8_t PmemUpdate128(struct cam_ois_ctrl_t *o_ctrl, uint8_t fw_type)
{
	char fw_name[32] = {0};
	uint8_t data[BURST_LENGTH_UC + 2];
	uint32_t i, UlReadVal, UlCnt;
	uint8_t	ReadData[8];
	uint8_t	fw_data[8];
	long long CheckSumCode = UpDataCodeCheckSum_07_00;
	uint8_t *p = (uint8_t *)&CheckSumCode;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}
	if (fw_type == 0)
		snprintf(fw_name, 32, "%s.prog", o_ctrl->ois_name);
	else if (fw_type == 1)
		snprintf(fw_name, 32, "%s.coeff", o_ctrl->ois_name);
//--------------------------------------------------------------------------------
// 1. Write updata code to Pmem
//--------------------------------------------------------------------------------
	IOWrite32A(o_ctrl, FLASHROM_FLAMODE, 0x00000000, 0);
	RamWrite32A(o_ctrl, 0x3000, 0x00080000, 0); // Pmem address set
	download_fw(o_ctrl, fw_name, 0x4000, o_ctrl->opcode.fw_addr_type, 20, fw_data, 8, 0, 0);

//--------------------------------------------------------------------------------
// 2. Verify
//--------------------------------------------------------------------------------

	// Program RAMのCheckSumの起動
	data[0] = 0xF0;	//CmdID
	data[1] = 0x0E;	//CmdID
	data[2] = (unsigned char)((UpDataCodeSize_07_00 >> 8) & 0x000000FF);
	data[3] = (unsigned char)(UpDataCodeSize_07_00 & 0x000000FF);
	data[4] = 0x00;
	data[5] = 0x00;	//(LSB)

	CntWrt(o_ctrl, data, 6, 0);

	// CheckSum loop
	UlCnt = 0;
	do {
		msleep(1);
		if(UlCnt++ > 10) {
			IOWrite32A(o_ctrl, FLASHROM_FLAMODE , 0x00000002, 0);
			CAM_DBG(CAM_OIS, "[OISDEBUG] CheckSum loop timeout");
			return (0x21);	// No enough memory
		}
		RamRead32A(o_ctrl, 0x0088, &UlReadVal);	// PmCheck.ExecFlag
	} while ( UlReadVal != 0 );

	CntRd(o_ctrl, 0xF00E, ReadData, 8);

	IOWrite32A(o_ctrl, FLASHROM_FLAMODE , 0x00000002, 0);
	// CheckSuml ｻ (A Header define)
	for(i = 0;i < 8; i++) {
		CAM_DBG(CAM_OIS, "ReadData[%d] = 0x%x, fw_data[%d] = 0x%x",
			7-i, ReadData[7-i], i, *p);
		if(ReadData[7-i] != *p++) { // CheckSum Code
			return (0x22);	// verify ng
		}
	}
	return 0;
}

//********************************************************************************
// Function Name 	: ProgramFlash128_LongBurst
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: User Mat All Erase

// History			: First edition
//********************************************************************************
uint8_t ProgramFlash128_LongBurst(struct cam_ois_ctrl_t *o_ctrl)
{
	char fw_name[32] = {0};
	const struct firmware *fw = NULL;
	//uint32_t fw_addr;
	struct device *dev = &(o_ctrl->pdev->dev);
	int32_t rc = 0;
	uint16_t total_bytes = 0;

	uint32_t UlReadVal, UlCnt, UlNum;
	uint32_t i, j;
	uint16_t Remainder;
	uint8_t data[(BURST_LENGTH_FC + 2)];
	uint8_t UcOddEvn = 0;
	const uint8_t *NcFromVal = NULL;//BURST_LENGTH_FC;
	const uint8_t *NcFromVal1st = NULL;//ptr->FromCode;
	snprintf(fw_name, 32, "%s.mem", o_ctrl->ois_name);

	/* Load FW */
	rc = request_firmware(&fw, fw_name, dev);

	if (rc) {
		CAM_ERR(CAM_OIS, "Failed to locate %s", fw_name);
		return -1;
	} else {
		NcFromVal = (uint8_t *)fw->data + BURST_LENGTH_FC;
		NcFromVal1st = (uint8_t *)fw->data;
		total_bytes = fw->size;

		IOWrite32A(o_ctrl, 0xE0701C, 0x00000000, 0);
		RamWrite32A(o_ctrl, 0x067C, 0x000800ac, 0);	// F008 Update
		RamWrite32A(o_ctrl, 0x0680, 0x000800be, 0);	// F009 Update

		RamWrite32A(o_ctrl, 0xF007, 0x00000000, 0);	// FlashAccess Setup
	//	RamWrite32A(o_ctrl, 0xF00A, 0x00000000, 0);	// FromCmd.Addrの設定
		RamWrite32A(o_ctrl, 0xF00A, 0x00000030, 0);	// FromCmd.Addrの設定

		data[0] = 0xF0;		// CmdH
		data[1] = 0x08;		// CmdL

		for(i = 1;i < (total_bytes / BURST_LENGTH_FC) ; i++)
		{
			if( ++UcOddEvn >1 )
			  	UcOddEvn = 0;	// 奇数偶数Check
			if (UcOddEvn == 0)
				data[1] = 0x08;
			else
				data[1] = 0x09;
			UlNum = 2;
			CAM_DBG(CAM_OIS, "[OISDEBUG] step 1 write mem at %d", i);
			for(j=0 ; j < BURST_LENGTH_FC; j++){
				data[UlNum++] = *NcFromVal++;
			}
			UlCnt = 0;
			if(UcOddEvn == 0){
				do {
					RamRead32A(o_ctrl, 0xF00C, &UlReadVal);
					if( UlCnt++ > 100 ) {
						IOWrite32A(o_ctrl, 0xE0701C , 0x00000002, 0);
						return (0x41) ;				// write ng
					}
				} while ((UlReadVal & 0x00000004) != 0);
			} else {
				do {
					RamRead32A(o_ctrl, 0xF00C, &UlReadVal);
					if( UlCnt++ > 100 ) {
						IOWrite32A(o_ctrl, 0xE0701C , 0x00000002, 0);
						return (0x41) ;				// write ng
					}
				} while ((UlReadVal & 0x00000008) != 0);
			}

			CntWrt(o_ctrl, data, BURST_LENGTH_FC+2, 0);  // Cmd 2Byte.
		}
		CAM_DBG(CAM_OIS, "[OISDEBUG] step 1 done");
		Remainder = (total_bytes % BURST_LENGTH_FC ) / 64;
		for (i = 0; i < Remainder; i++)
		{
			if( ++UcOddEvn >1 )
				UcOddEvn = 0;
			if (UcOddEvn == 0)
				data[1] = 0x08;
			else
				data[1] = 0x09;
			UlNum = 2;
			CAM_DBG(CAM_OIS, "[OISDEBUG] step 2 write mem at %d", i);
			for(j=0 ; j < BURST_LENGTH_FC; j++) {
				data[UlNum++] = *NcFromVal++;
			}
			UlCnt = 0;
			if(UcOddEvn == 0){
				do {
					RamRead32A(o_ctrl, 0xF00C, &UlReadVal);
					if( UlCnt++ > 100 ) {
						IOWrite32A(o_ctrl, 0xE0701C , 0x00000002, 0);
						return (0x41);	// write ng
					}
				} while ((UlReadVal & 0x00000004) != 0);
			} else {
				do {
					RamRead32A(o_ctrl, 0xF00C, &UlReadVal);
					if( UlCnt++ > 100 ) {
						IOWrite32A(o_ctrl, 0xE0701C , 0x00000002, 0);
						return (0x41);	// write ng
					}
				} while ((UlReadVal & 0x00000008) != 0);
			}
			CntWrt(o_ctrl, data, BURST_LENGTH_FC+2, 0);  // Cmd 2Byte.
		}
		CAM_DBG(CAM_OIS, "[OISDEBUG] step 2 done");
		UlCnt = 0;
		do {
	//		msleep(4);
			msleep(1);
			RamRead32A(o_ctrl, 0xF00C, &UlReadVal);
			if( UlCnt++ > 10 ) {
				IOWrite32A(o_ctrl, 0xE0701C, 0x00000002, 0);
				return (0x42);	// write ng
			}
		} while ( (UlReadVal & 0x0000000C) != 0 );

		/* write magic code */
		RamWrite32A(o_ctrl, 0xF00A, 0x00000000, 0);
		data[1] = 0x08;	// CmdL
		UlNum = 2;
		for(j=0 ; j < BURST_LENGTH_FC; j++) {
			data[UlNum++] = *NcFromVal1st++;
		}

		UlCnt = 0;
		do{
			RamRead32A(o_ctrl, 0xF00C, &UlReadVal);
			if (UlCnt++ > 100) {
				IOWrite32A(o_ctrl, 0xE0701C , 0x00000002, 0);
				return (0x41);	// write ng
			}
		} while (UlReadVal != 0);

		CntWrt(o_ctrl, data, BURST_LENGTH_FC+2, 0);  // Cmd 2Byte.

		UlCnt = 0;
		do{
			msleep(1);
			RamRead32A(o_ctrl, 0xF00C, &UlReadVal);
			if( UlCnt++ > 10 ) {
				IOWrite32A(o_ctrl, 0xE0701C , 0x00000002, 0);
				return (0x42);	// write ng
			}
		} while ((UlReadVal & 0x0000000C) != 0);

		IOWrite32A(o_ctrl, 0xE0701C, 0x00000002, 0);
		release_firmware(fw);
	}
	return 0;
}


//********************************************************************************
// Function Name 	: ProgramFlash128_Standard
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: User Mat All Erase

// History			: First edition
//********************************************************************************
#if (BURST_LENGTH_FC == 32) || (BURST_LENGTH_FC == 64)
uint8_t ProgramFlash128_Standard(struct cam_ois_ctrl_t *o_ctrl)
{
	char fw_name[32] = {0};
	const struct firmware *fw = NULL;
	//uint32_t fw_addr;
	struct device *dev = &(o_ctrl->pdev->dev);
	int32_t rc = 0;
	uint16_t total_bytes = 0;

	uint32_t UlReadVal, UlCnt, UlNum;
	uint8_t	data[(BURST_LENGTH_FC + 3)];
	uint32_t i, j;
	const uint8_t *NcFromVal = NULL;//ptr->FromCode + 64;
	const uint8_t *NcFromVal1st = NULL;//ptr->FromCode;
	uint8_t UcOddEvn;

	snprintf(fw_name, 32, "%s.mem", o_ctrl->ois_name);

	/* Load FW */
	rc = request_firmware(&fw, fw_name, dev);

	if (rc) {
		CAM_ERR(CAM_OIS, "Failed to locate %s", fw_name);
		return -1;
	} else {
		NcFromVal = (uint8_t *)fw->data + BURST_LENGTH_FC;
		NcFromVal1st = (uint8_t *)fw->data;
		total_bytes = fw->size;

		IOWrite32A(o_ctrl, 0xE0701C , 0x00000000, 0);
		RamWrite32A(o_ctrl, 0xF007, 0x00000000, 0);	// FlashAccess Setup
		RamWrite32A(o_ctrl, 0xF00A, 0x00000010, 0);	// FromCmd.Addrの設定
		data[0] = 0xF0;	// CmdH
		data[1] = 0x08;	// CmdL
		data[2] = 0x00;	// FromCmd.BufferAのアドレス

		for (i = 1; i< (total_bytes / 64); i++)
		{
			if ( ++UcOddEvn >1 )
			  	UcOddEvn = 0;	// 奇数偶数Check
			if (UcOddEvn == 0)
				data[1] = 0x08;
			else
				data[1] = 0x09;
			CAM_DBG(CAM_OIS, "[%d] UcOddEvn=%d, data[1]= %d", i, data[1], NcFromVal);

#if (BURST_LENGTH_FC == 32)
			// 32Byte
			data[2] = 0x00;
			UlNum = 3;
			for(j = 0; j < BURST_LENGTH_FC; j++){
				data[UlNum++] = *NcFromVal++;
			}
			CntWrt(o_ctrl, data, BURST_LENGTH_FC+3, 0);  // Cmd 3Byte.
		  	data[2] = 0x20;		//+32Byte
			UlNum = 3;
			for(j=0 ; j < BURST_LENGTH_FC; j++){
				data[UlNum++] = *NcFromVal++;
			}
			CntWrt(o_ctrl, data, BURST_LENGTH_FC+3, 0);  // Cmd 3Byte.
#elif (BURST_LENGTH_FC == 64)
			UlNum = 3;
			for(j = 0; j < BURST_LENGTH_FC; j++){
				data[UlNum++] = *NcFromVal++;
			}
			CntWrt(o_ctrl, data, BURST_LENGTH_FC+3, 0);  // Cmd 3Byte.
#endif
			RamWrite32A(o_ctrl, 0xF00B, 0x00000010, 0);	// FromCmd.Length
			UlCnt = 0;
			if (UcOddEvn == 0){
				do{
					RamRead32A(o_ctrl, 0xF00C, &UlReadVal);
					if( UlCnt++ > 250 ) {
						IOWrite32A(o_ctrl, 0xE0701C, 0x00000002, 0);
						return (0x41);	// write ng
					}
				} while (UlReadVal != 0);
			 	RamWrite32A(o_ctrl, 0xF00C, 0x00000004, 0);	// FromCmd.Control
			} else {
				do {
					RamRead32A(o_ctrl, 0xF00C, &UlReadVal);	// FromCmd.Control
					if ( UlCnt++ > 250 ) {
						IOWrite32A(o_ctrl, 0xE0701C , 0x00000002, 0);
						return (0x41);	// write ng
					}
				} while (UlReadVal != 0);
				RamWrite32A(o_ctrl, 0xF00C, 0x00000008, 0);	// FromCmd.Control
			}
		}

		UlCnt = 0;
		do {
			msleep(1);
			RamRead32A(o_ctrl, 0xF00C, &UlReadVal);	// FromCmd.Control
			if( UlCnt++ > 250 ) {
				IOWrite32A(o_ctrl, 0xE0701C , 0x00000002, 0);
				return (0x41);	// write ng
			}
		} while ( (UlReadVal & 0x0000000C) != 0 );

		{	/* write magic code */
			RamWrite32A(o_ctrl, 0xF00A, 0x00000000, 0);	// FromCmd.Addr
			data[1] = 0x08;
//			data[1] = 0x09;
			CAM_DBG(CAM_OIS, "[%d]UcOddEvn= %d, data[1]= %d", 0, data[1], NcFromVal1st );

#if (BURST_LENGTH_FC == 32)
			// 32Byteならば、2回に分けて送らないといけない。
			data[2] = 0x00;
			UlNum = 3;
			for(j=0 ; j < BURST_LENGTH_FC; j++){
				data[UlNum++] = *NcFromVal1st++;
			}
			CntWrt(o_ctrl, data, BURST_LENGTH_FC+3, 0);  // Cmd 3Byte.
		  	data[2] = 0x20;		//+32Byte
			UlNum = 3;
			for(j=0 ; j < BURST_LENGTH_FC; j++){
				data[UlNum++] = *NcFromVal1st++;
			}
			CntWrt(o_ctrl, data, BURST_LENGTH_FC+3, 0);  // Cmd 3Byte.
#elif (BURST_LENGTH_FC == 64)
			data[2] = 0x00;
			UlNum = 3;
			for(j=0 ; j < BURST_LENGTH_FC; j++){
				data[UlNum++] = *NcFromVal1st++;
			}
			CntWrt(o_ctrl, data, BURST_LENGTH_FC+3, 0);  // Cmd 3Byte.
#endif

			RamWrite32A(o_ctrl, 0xF00B, 0x00000010, 0);	// FromCmd.Length
			UlCnt = 0;
			do{
				RamRead32A(o_ctrl, 0xF00C, &UlReadVal);	// FromCmd.Control
				if( UlCnt++ > 250 ) {
					IOWrite32A(o_ctrl, 0xE0701C , 0x00000002, 0);
					return (0x41);	// write ng
				}
			} while ( UlReadVal != 0 );
		 	RamWrite32A(o_ctrl, 0xF00C, 0x00000004, 0);	// FromCmd.Control
//		 	RamWrite32A(o_ctrl, 0xF00C, 0x00000008, 0);	// FromCmd.Control
		}

		UlCnt = 0;
		do {
			msleep(1);
			RamRead32A(o_ctrl, 0xF00C, &UlReadVal);	// FromCmd.Control
			if( UlCnt++ > 250 ) {
				IOWrite32A(o_ctrl, 0xE0701C, 0x00000002, 0);
				return (0x41);	// write ng
			}
		} while ( (UlReadVal & 0x0000000C) != 0 );

		IOWrite32A(o_ctrl, 0xE0701C , 0x00000002, 0);
	}
	return( 0 );
}
#endif

uint8_t MatVerify(struct cam_ois_ctrl_t *o_ctrl, uint32_t FwChecksum, uint32_t FwChecksumSize)
{
	uint32_t UlReadVal, UlCnt ;

	IOWrite32A(o_ctrl, 0xE0701C, 0x00000000, 0);
	RamWrite32A(o_ctrl, 0xF00A, 0x00000000, 0);
	RamWrite32A(o_ctrl, 0xF00D, FwChecksumSize, 0);
	RamWrite32A(o_ctrl, 0xF00C, 0x00000100, 0);
	msleep(6);
	UlCnt = 0;

	do {
		RamRead32A(o_ctrl, 0xF00C, &UlReadVal);
		if (UlCnt++ > 10) {
			IOWrite32A(o_ctrl, 0xE0701C , 0x00000002, 0);
			return (0x51);	// check sum excute ng
		}
		msleep(1);
	} while (UlReadVal != 0);

	RamRead32A(o_ctrl, 0xF00D, &UlReadVal);

	if (UlReadVal != FwChecksum) {
		IOWrite32A(o_ctrl, 0xE0701C , 0x00000002, 0);
		return(0x52);
	}


	CAM_DBG(CAM_OIS, "UserMat Verify OK");
	// CoreReset
	// CORE_RST[12], MC_IGNORE2[10] = 1 PRAMSEL[7:6]=01b
	IOWrite32A(o_ctrl, SYSDSP_REMAP, 0x00001000, 0);
	msleep(15);
	IORead32A(o_ctrl, ROMINFO,(uint32_t *)&UlReadVal);
	CAM_DBG(CAM_OIS, "UlReadVal = %x", UlReadVal);
	if (UlReadVal != 0x0A)
		return( 0x53 );

	CAM_DBG(CAM_OIS, "Remap OK");
	return 0;
}

//********************************************************************************
// Function Name 	: WrGyroGainData
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Flash write gyro gain data
// History			: First edition
//********************************************************************************
uint8_t WrGyroGainData( struct cam_ois_ctrl_t *o_ctrl, uint8_t UcMode )
{
	uint32_t	UlMAT0[32];
	uint32_t	UlReadGxzoom , UlReadGyzoom;
	uint8_t ans = 0, i;
	uint16_t	UsCkVal,UsCkVal_Bk ;

        CAM_DBG(CAM_OIS, "[WrGyroGainData]BEGIN WrGyroGainData");
	/* Back up ******************************************************/
	ans =FlashMultiRead( o_ctrl,INF_MAT0, 0, UlMAT0, 32 );	// check sum ｿｿ
	if( ans )	return( 1 );

	/* Erase   ******************************************************/
	ans = FlashBlockErase( o_ctrl,INF_MAT0 , 0 );	// all erase
	if( ans != 0 )	return( 2 ) ;							// Unlock Code Set

	for(i=0;i<32;i++){
		CAM_DBG(CAM_OIS, "[WrGyroGainData] the origin date in flash [ %d ] = %08x\n",i, UlMAT0[i] );
	}

	/* modify   *****************************************************/
	if( UcMode ){	// write
		RamRead32A( o_ctrl, GyroFilterTableX_gxzoom , &UlReadGxzoom ) ;
		RamRead32A( o_ctrl, GyroFilterTableY_gyzoom , &UlReadGyzoom ) ;

		UlMAT0[CALIBRATION_STATUS] &= ~( GYRO_GAIN_FLG );
		UlMAT0[GYRO_GAIN_X] = UlReadGxzoom;
		UlMAT0[GYRO_GAIN_Y] = UlReadGyzoom;
	}else{
		UlMAT0[CALIBRATION_STATUS] |= GYRO_GAIN_FLG;
		UlMAT0[GYRO_GAIN_X] = 0x3FFFFFFF;
		UlMAT0[GYRO_GAIN_Y] = 0x3FFFFFFF;
	}
	/* calcurate check sum ******************************************/
	UsCkVal = 0;
	for( i=0; i < 31; i++ ){
		UsCkVal +=  (uint8_t)(UlMAT0[i]>>0);
		UsCkVal +=  (uint8_t)(UlMAT0[i]>>8);
		UsCkVal +=  (uint8_t)(UlMAT0[i]>>16);
		UsCkVal +=  (uint8_t)(UlMAT0[i]>>24);
		CAM_DBG(CAM_OIS, "[WrGyroGainData] calcurate check sum UlMAT0 [ %d ] = %08x\n",i, UlMAT0[i] );
	}
	// Remainder
	UsCkVal +=  (uint8_t)(UlMAT0[i]>>0);
	UsCkVal +=  (uint8_t)(UlMAT0[i]>>8);
	UlMAT0[MAT0_CKSM] = ((uint32_t)UsCkVal<<16) | ( UlMAT0[MAT0_CKSM] & 0x0000FFFF);

	CAM_DBG(CAM_OIS, "[WrGyroGainData] calcurate check sum UlMAT0 [ %d ] = %08x\n",i, UlMAT0[i] );
	/* update ******************************************************/
	ans = FlashBlockWrite( o_ctrl,INF_MAT0 , 0 , UlMAT0 );
	if( ans != 0 )	return( 3 ) ;							// Unlock Code Set
	ans = FlashBlockWrite( o_ctrl,INF_MAT0 , (uint32_t)0x10 , &UlMAT0[0x10] );
	if( ans != 0 )	return( 3 ) ;							// Unlock Code Set
	/* Verify ******************************************************/
	UsCkVal_Bk = UsCkVal;
	ans =FlashMultiRead( o_ctrl,INF_MAT0, 0, UlMAT0, 32 );	// check sum ｿｿ
	if( ans )	return( 4 );

	UsCkVal = 0;
	for( i=0; i < 31; i++ ){
		UsCkVal +=  (uint8_t)(UlMAT0[i]>>0);
		UsCkVal +=  (uint8_t)(UlMAT0[i]>>8);
		UsCkVal +=  (uint8_t)(UlMAT0[i]>>16);
		UsCkVal +=  (uint8_t)(UlMAT0[i]>>24);
	}
	// Remainder
	UsCkVal +=  (uint8_t)(UlMAT0[i]>>0);
	UsCkVal +=  (uint8_t)(UlMAT0[i]>>8);
	CAM_DBG(CAM_OIS, "[WrGyroGainData][RVAL]:[BVal]=[%04x]:[%04x]\n",UsCkVal, UsCkVal_Bk );

	if( UsCkVal != UsCkVal_Bk )		return(5);

        CAM_DBG(CAM_OIS, "[WrGyroGainData] compelete ");
	return(0);
}

uint8_t download_fw(
	struct cam_ois_ctrl_t *o_ctrl,
	char* firmware_name,
	uint32_t addr,
	uint32_t addr_type,
	uint32_t write_stride,
	uint8_t *read_data,
	uint32_t read_length,
	uint8_t is_pingpong,
	uint32_t pingpong_addr)
{

	uint16_t                           total_bytes = 0;
	uint8_t                           *ptr = NULL;
	int32_t                            rc = 0, cnt, i;
	uint32_t                           pingpong_cnt = 0;
	uint32_t                           fw_size;
	uint32_t                           fw_addr;
	const struct firmware             *fw = NULL;
	const char                        *fw_name = NULL;
	struct device                     *dev = &(o_ctrl->pdev->dev);
	struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
	void                              *vaddr = NULL;

	fw_addr = addr;
	fw_name = firmware_name;

	/* Load FW */
	rc = request_firmware(&fw, fw_name, dev);
	if (rc) {
		CAM_ERR(CAM_OIS, "Failed to locate %s", fw_name);
	} else {
		total_bytes = fw->size;
		i2c_reg_setting.addr_type = addr_type;
		i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		i2c_reg_setting.size = total_bytes;
		i2c_reg_setting.delay = 0;
		fw_size = sizeof(struct cam_sensor_i2c_reg_array) * total_bytes;
		vaddr = vmalloc(fw_size);

		if (!vaddr) {
		    CAM_ERR(CAM_OIS,
		    "Failed in allocating i2c_array: fw_size: %u", fw_size);
			release_firmware(fw);
			return -ENOMEM;
		}

		i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) vaddr;

		ptr = (uint8_t *)fw->data;
		if (read_data) {
			for (i = 0; i < read_length; i++) {
				read_data[i] = *(ptr + i);
			}
		}
		for (i = 0; i < total_bytes;) {
			for (cnt = 0; cnt < write_stride && i < total_bytes;
				cnt++, ptr++, i++) {
				i2c_reg_setting.reg_setting[cnt].reg_addr = fw_addr;
				i2c_reg_setting.reg_setting[cnt].reg_data = *ptr;
				i2c_reg_setting.reg_setting[cnt].delay = 0;
				i2c_reg_setting.reg_setting[cnt].data_mask = 0;
				CAM_DBG(CAM_OIS, "[OISDEBUG] prog byte[%d] addr 0x%04x = 0x%04x", i, fw_addr, *ptr);
			}
			i2c_reg_setting.size = cnt;

			rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
				&i2c_reg_setting, 1);
			if (rc < 0) {
				CAM_ERR(CAM_OIS, "OIS FW %s download failed %d", fw_name, rc);
				break;
			}
			if (is_pingpong) {
				pingpong_cnt++;
				if (pingpong_cnt % 2) {
					fw_addr = addr;
				} else {
					fw_addr = pingpong_addr;
				}
			}
		}

		vfree(vaddr);
		vaddr = NULL;
		fw_size = 0;
		release_firmware(fw);
	}
	return 0;
}
