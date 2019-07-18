/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#ifndef _CAM_CDM_UTIL_H_
#define _CAM_CDM_UTIL_H_

#define CAM_CDM_SW_CMD_COUNT    2
#define CAM_CMD_LENGTH_MASK     0xFFFF
#define CAM_CDM_COMMAND_OFFSET  24

#define CAM_CDM_DMI_DATA_HI_OFFSET   8
#define CAM_CDM_DMI_DATA_LO_OFFSET   12

enum cam_cdm_command {
	CAM_CDM_CMD_UNUSED = 0x0,
	CAM_CDM_CMD_DMI = 0x1,
	CAM_CDM_CMD_NOT_DEFINED = 0x2,
	CAM_CDM_CMD_REG_CONT = 0x3,
	CAM_CDM_CMD_REG_RANDOM = 0x4,
	CAM_CDM_CMD_BUFF_INDIRECT = 0x5,
	CAM_CDM_CMD_GEN_IRQ = 0x6,
	CAM_CDM_CMD_WAIT_EVENT = 0x7,
	CAM_CDM_CMD_CHANGE_BASE = 0x8,
	CAM_CDM_CMD_PERF_CTRL = 0x9,
	CAM_CDM_CMD_DMI_32 = 0xa,
	CAM_CDM_CMD_DMI_64 = 0xb,
	CAM_CDM_CMD_PRIVATE_BASE = 0xc,
	CAM_CDM_CMD_SWD_DMI_32 = (CAM_CDM_CMD_PRIVATE_BASE + 0x64),
	CAM_CDM_CMD_SWD_DMI_64 = (CAM_CDM_CMD_PRIVATE_BASE + 0x65),
	CAM_CDM_CMD_PRIVATE_BASE_MAX = 0x7F
};

/**
 * struct cam_cdm_utils_ops - Camera CDM util ops
 *
 * @cdm_get_cmd_header_size: Returns the size of the given command header
 *                           in DWORDs.
 *      @command Command ID
 *      @return Size of the command in DWORDs
 *
 * @cdm_required_size_reg_continuous: Calculates the size of a reg-continuous
 *                                    command in dwords.
 *      @numVals Number of continuous values
 *      @return Size in dwords
 *
 * @cdm_required_size_reg_random: Calculates the size of a reg-random command
 *                                in dwords.
 *      @numRegVals  Number of register/value pairs
 *      @return Size in dwords
 *
 * @cdm_required_size_dmi: Calculates the size of a DMI command in dwords.
 *      @return Size in dwords
 *
 * @cdm_required_size_genirq: Calculates size of a Genirq command in dwords.
 *      @return Size in dwords
 *
 * @cdm_required_size_indirect: Calculates the size of an indirect command
 *                              in dwords.
 *      @return Size in dwords
 *
 * @cdm_required_size_changebase: Calculates the size of a change-base command
 *                                in dwords.
 *      @return Size in dwords
 *
 * @cdm_offsetof_dmi_addr: Returns the offset of address field in the DMI
 *                         command header.
 *      @return Offset of addr field
 *
 * @cdm_offsetof_indirect_addr: Returns the offset of address field in the
 *                              indirect command header.
 *      @return Offset of addr field
 *
 * @cdm_write_regcontinuous: Writes a command into the command buffer.
 *      @pCmdBuffer:  Pointer to command buffer
 *      @reg: Beginning of the register address range where
 *            values will be written.
 *      @numVals: Number of values (registers) that will be written
 *      @pVals : An array of values that will be written
 *      @return Pointer in command buffer pointing past the written commands
 *
 * @cdm_write_regrandom: Writes a command into the command buffer in
 *                       register/value pairs.
 *      @pCmdBuffer: Pointer to command buffer
 *      @numRegVals: Number of register/value pairs that will be written
 *      @pRegVals: An array of register/value pairs that will be written
 *                 The even indices are registers and the odd indices
 *                 arevalues, e.g., {reg1, val1, reg2, val2, ...}.
 *      @return Pointer in command buffer pointing past the written commands
 *
 * @cdm_write_dmi: Writes a DMI command into the command bufferM.
 *      @pCmdBuffer: Pointer to command buffer
 *      @dmiCmd: DMI command
 *      @DMIAddr: Address of the DMI
 *      @DMISel: Selected bank that the DMI will write to
 *      @length: Size of data in bytes
 *      @return Pointer in command buffer pointing past the written commands
 *
 * @cdm_write_indirect: Writes a indirect command into the command buffer.
 *      @pCmdBuffer: Pointer to command buffer
 *      @indirectBufferAddr: Device address of the indirect cmd buffer.
 *      @length: Size of data in bytes
 *      @return Pointer in command buffer pointing past the written commands
 *
 * @cdm_write_changebase: Writes a changing CDM (address) base command into
 *                        the command buffer.
 *      @pCmdBuffer: Pointer to command buffer
 *      @base: New base (device) address
 *      @return Pointer in command buffer pointing past the written commands
 *
 * @cdm_write_genirq:  Writes a gen irq command into the command buffer.
 *      @pCmdBuffer: Pointer to command buffer
 *      @userdata: userdata or cookie return by hardware during irq.
 */
struct cam_cdm_utils_ops {
uint32_t (*cdm_get_cmd_header_size)(unsigned int command);
uint32_t (*cdm_required_size_reg_continuous)(uint32_t  numVals);
uint32_t (*cdm_required_size_reg_random)(uint32_t numRegVals);
uint32_t (*cdm_required_size_dmi)(void);
uint32_t (*cdm_required_size_genirq)(void);
uint32_t (*cdm_required_size_indirect)(void);
uint32_t (*cdm_required_size_changebase)(void);
uint32_t (*cdm_offsetof_dmi_addr)(void);
uint32_t (*cdm_offsetof_indirect_addr)(void);
uint32_t* (*cdm_write_regcontinuous)(
	uint32_t *pCmdBuffer,
	uint32_t reg,
	uint32_t numVals,
	uint32_t *pVals);
uint32_t *(*cdm_write_regrandom)(
	uint32_t *pCmdBuffer,
	uint32_t numRegVals,
	uint32_t *pRegVals);
uint32_t *(*cdm_write_dmi)(
	uint32_t *pCmdBuffer,
	uint8_t  dmiCmd,
	uint32_t DMIAddr,
	uint8_t  DMISel,
	uint32_t dmiBufferAddr,
	uint32_t length);
uint32_t *(*cdm_write_indirect)(
	uint32_t *pCmdBuffer,
	uint32_t indirectBufferAddr,
	uint32_t length);
uint32_t *(*cdm_write_changebase)(
	uint32_t *pCmdBuffer,
	uint32_t base);
void (*cdm_write_genirq)(
	uint32_t *pCmdBuffer,
	uint32_t  userdata);
};

#endif /* _CAM_CDM_UTIL_H_ */
