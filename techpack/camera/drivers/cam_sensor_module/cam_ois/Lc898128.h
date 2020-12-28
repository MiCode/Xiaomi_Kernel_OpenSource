/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */
#ifndef _CAM_LC898218_H_
#define _CAM_LC898218_H_

#include "OisLc898128.h"
//********************************************************************************
// Function Name 	: UnlockCodeSet
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: <Flash Memory> Unlock Code Set
// History			: First edition
//********************************************************************************
uint8_t UnlockCodeSet(struct cam_ois_ctrl_t *o_ctrl);

//********************************************************************************
// Function Name 	: WritePermission
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: LC898128 Command
// History			: First edition 						2018.05.15
//********************************************************************************
void WritePermission(struct cam_ois_ctrl_t *o_ctrl);

//********************************************************************************
// Function Name 	: AddtionalUnlockCodeSet
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: LC898128 Command
// History			: First edition 						2018.05.15
//********************************************************************************
void AddtionalUnlockCodeSet(struct cam_ois_ctrl_t *o_ctrl);

//********************************************************************************
// Function Name 	: EraseUserMat128
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: User Mat All Erase

// History			: First edition
//********************************************************************************
uint8_t EraseUserMat128(struct cam_ois_ctrl_t *o_ctrl, uint8_t StartBlock, uint8_t EndBlock);

//********************************************************************************
// Function Name 	: UnlockCodeClear
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: <Flash Memory> Clear Unlock Code
// History			: First edition
//********************************************************************************
uint8_t UnlockCodeClear(struct cam_ois_ctrl_t *o_ctrl);

//********************************************************************************
// Function Name 	: ProgramFlash128_LongBurst
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: User Mat All Erase

// History			: First edition
//********************************************************************************
uint8_t ProgramFlash128_LongBurst(struct cam_ois_ctrl_t *o_ctrl);

//********************************************************************************
// Function Name 	: ProgramFlash128_Standard
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: User Mat All Erase

// History			: First edition
//********************************************************************************
#if (BURST_LENGTH_FC == 32) || (BURST_LENGTH_FC == 64)
uint8_t ProgramFlash128_Standard(struct cam_ois_ctrl_t *o_ctrl);
#endif

//********************************************************************************
// Function Name 	: Mat2ReWrite
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Mat2 re-write function
// History			: First edition
//********************************************************************************
uint8_t Mat2ReWrite(struct cam_ois_ctrl_t *o_ctrl);

uint8_t MatVerify(struct cam_ois_ctrl_t *o_ctrl, uint32_t FwChecksum, uint32_t FwChecksumSize);

uint32_t CheckFwValid(struct cam_ois_ctrl_t *o_ctrl,  uint8_t CurrentFwVersion);

uint32_t DrvOffAdj(struct cam_ois_ctrl_t *o_ctrl);

uint32_t CheckDrvOffAdj(struct cam_ois_ctrl_t *o_ctrl);

uint8_t CoreResetwithoutMC128(struct cam_ois_ctrl_t *o_ctrl);

uint8_t PmemUpdate128(struct cam_ois_ctrl_t *o_ctrl, uint8_t fw_type);

uint8_t WrGyroGainData(struct cam_ois_ctrl_t *o_ctrl, uint8_t UcMode);

uint8_t download_fw(
	struct cam_ois_ctrl_t *o_ctrl,
	char* firmware_name,
	uint32_t addr,
	uint32_t addr_type,
	uint32_t write_stride,
	uint8_t *read_data,
	uint32_t read_length,
	uint8_t is_pingpong,
	uint32_t pingpong_addr);

//uint8_t SetAngleCorrection(struct cam_ois_ctrl_t *o_ctrl, float DegreeGap, uint8_t SelectAct, uint8_t Arrangement);
//void SetGyroCoef(struct cam_ois_ctrl_t *o_ctrl, uint8_t UcCnvF);
//void SetAccelCoef(struct cam_ois_ctrl_t *o_ctrl, uint8_t UcCnvF);

#endif
/* _CAM_LC898218_H_ */
