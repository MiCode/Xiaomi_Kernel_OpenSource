/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#ifndef __iwl_sdio_al_h__
#define __iwl_sdio_al_h__

#define IWL_SDIO_BLOCK_SIZE 512

/*
 * IWL_SDIO_INTR_CAUSE_REG
 * FW uses this register to assert an interrupt to the host driver.
 *
 * BIT 0: Read data ready int
 * Read Ready Interrupt:
 * If set to 1, it indicates that the device has data ready to send to the SD
 * Host. The bit remains set to 1 until the SD Host writes 1 to the
 * corresponding bit in the Interrupt identification register.
 * This bit is set to one whenever device sets the read_data_rdy bit in
 * read_data_rdy register.
 *
 * BIT 1: Read error
 * Read Error Interrupt:
 * If set to 1, it indicates that the device has sent an error response during
 * data transaction and the host has to retry the same transaction to
 * prevent data loss.
 *
 * BIT 2: Message from device
 * device Message Interrupt:
 * If set to 1, it indicates that the device has programmed the device General
 * Purpose Register (Message from device to SD Host).
 * On receiving this Interrupt, the SD Host will read the device General purpose
 * register to find the message from device.
 *
 * BIT 3: ACK to SD Host
 * Acknowledgment to SD Host:
 * If set to 1, it indicates that the device has read the message.
 *
 * BIT 7:4
 * Reserved for future use
*/
#define IWL_SDIO_INTR_CAUSE_REG			0x08
enum iwl_sdio_inter_cause {
	IWL_SDIO_INTR_DATA_READY		= 0x1,
	IWL_SDIO_INTR_READ_ERROR		= 0x2,
	IWL_SDIO_INTR_D2H_GPR_MSG		= 0x4,
	IWL_SDIO_INTR_H2D_GPR_MSG_ACK		= 0x8,
};
#define IWL_SDIO_INTR_CAUSE_CLEAR_ALL_VAL	0xF
#define IWL_SDIO_INTR_CAUSE_VALID_MASK \
			(IWL_SDIO_INTR_DATA_READY | \
			 IWL_SDIO_INTR_READ_ERROR | \
			 IWL_SDIO_INTR_D2H_GPR_MSG | \
			 IWL_SDIO_INTR_H2D_GPR_MSG_ACK)

/*
 * IWL_SDIO_INTR_ENABLE_MASK_REG
 * Host driver uses the int_mask register to control which bit of INTA
 * register can force assertion of an interrupt.
 * 31:24		 Interrupt masking
 * 23:8:		 Reserved
 * 7:0:		 Interrupt masking
 * Value of 1 - Interrupt is enabled
 * Value of 0 - Interrupt is masked
 */
#define IWL_SDIO_INTR_ENABLE_MASK_REG		0x09
#define IWL_SDIO_INTR_ENABLE_ALL		0x0F
#define IWL_SDIO_INTR_DISABLE_ALL		0x0

/*
* IWL_SDIO_READ_COUNT_REG
* This register contains the transfer count value programmable by ARM for
* a read transaction.
* 20:0		ahb_xfer_cnt: count value for a read transaction.
* 31:31		reserved for future use
* Since the value returned by IWL_SDIO_READ_COUNT_REG is always a multiple of
* 256, the LSB is always 0 and this can allow access with CMD52 (single byte).
*/
#define IWL_SDIO_READ_COUNT_REG		(0x0c)
#define IWL_SDIO_READ_COUNT_BYTE_1	(IWL_SDIO_READ_COUNT_REG + 1)
#define IWL_SDIO_READ_COUNT_MSK		(0x1FFFFF)

/*
 * IWL_SDIO_H2D_GP_REG
 * SD Host General Purpose Register - H2D.
 * An Interrupt will be asserted to the device, whenever SD Host writes into
 * this register indicating that there is a message for device.
*/
#define IWL_SDIO_H2D_GP_REG			0x24
#define IWL_SDIO_RETENTION_REG			(IWL_SDIO_H2D_GP_REG + 0x3)
#define IWL_SDIO_DISABLE_RETENTION_VAL		BIT(7)
#define IWL_SDIO_ENABLE_RETENTION_MASK		~IWL_SDIO_DISABLE_RETENTION_VAL

/*
 * enum iwl_sdio_d2h_gpr_msg - messages coming from the SDIO core
 */
enum iwl_sdio_d2h_gpr_msg {
	IWL_SDIO_MSG_SW_GP0		 = BIT(0),
	IWL_SDIO_MSG_SW_GP1		 = BIT(1),
	IWL_SDIO_MSG_SW_GP2		 = BIT(2),
	IWL_SDIO_MSG_SW_GP3		 = BIT(3),
	IWL_SDIO_MSG_SW_GP4		 = BIT(4),
	IWL_SDIO_MSG_SW_GP5		 = BIT(5),
	IWL_SDIO_MSG_SW_GP6		 = BIT(6),
	IWL_SDIO_MSR_RFKILL		 = BIT(7),
	IWL_SDIO_MSG_WR_IN_LOW_RETENTION = BIT(14),
	IWL_SDIO_MSG_WR_ABORT		 = BIT(15),
	IWL_SDIO_MSG_RD_ABORT		 = BIT(16),
	IWL_SDIO_MSG_TARG_BAD_LEN	 = BIT(17),
	IWL_SDIO_MSG_TARG_BAD_ADDR	 = BIT(18),
	IWL_SDIO_MSG_TRANS_BAD_SIZE	 = BIT(19),
	IWL_SDIO_MSG_H2D_WDT_EXPIRE	 = BIT(20),
	IWL_SDIO_MSG_TARG_IN_PROGRESS	 = BIT(21),
	IWL_SDIO_MSG_BAD_OP_CODE	 = BIT(22),
	IWL_SDIO_MSG_BAD_SIG		 = BIT(23),
	IWL_SDIO_MSG_GP_INT		 = BIT(24),
	IWL_SDIO_MSG_LMAC_SW_ERROR	 = BIT(25),
	IWL_SDIO_MSG_SCD_ERROR		 = BIT(26),
	IWL_SDIO_MSG_FH_TX_INT		 = BIT(27),
	IWL_SDIO_MSG_LMAC_HW_ERROR	 = BIT(29),
	IWL_SDIO_MSG_VALID_ALL		 = 0x2FFFC0FF,
	IWL_SDIO_MSG_SDTM_ALL		 = 0x00FFFF00,
	IWL_SDIO_MSG_INTA_CSR_ALL	 = 0x2F0000FF,
};

/*
 * IWL_SDIO_D2H_GP_REG
 * SD Host General Purpose Register - D2H.
 * An Interrupt will be asserted to the device, whenever SD Host writes into
 * this register indicating that there is a message from the device.
*/
#define IWL_SDIO_D2H_GP_REG			0x28

/*
 * SDIO SDTM (SDIO Transaction Manager) registers and config values
 */
#define IWL_SDIO_CONFIG_BASE_ADDRESS		0x10050
#define IWL_SDIO_WATCH_DOG_TIMER_TIMEOUT_VALUE	0x20
#define IWL_SDIO_SCD_WR_PTR_ADDR                0x460

#define IWL_SDIO_SF_MEM_TFD_OFFSET		0x0000
#define IWL_SDIO_SF_MEM_TFDI_OFFSET		0x1000
#define IWL_SDIO_SF_MEM_BC_OFFSET		0x1800
#define IWL_SDIO_SF_MEM_TG_BUF_OFFSET		0x3800
#define IWL_SDIO_SF_MEM_ADMA_DSC_OFFSET		0x3A00
#define IWL_SDIO_SF_MEM_TB_OFFSET		0x3B00

/* this is Silicon issue will fix in B0 step */
#define FAMILY_8000_REDUCED_MEM_SIZE	32768

#define IWL_SDIO_8000_SF_MEM_SIZE	0x20000

#define IWL_SDIO_8000_SF_MEM_BASE_ADDR		0x93000
#define IWL_SDIO_8000B_SF_MEM_BASE_ADDR	0x400000

#define IWL_SDIO_8000_SF_MEM_TFD_BASE_ADDR \
	(IWL_SDIO_8000_SF_MEM_BASE_ADDR + IWL_SDIO_SF_MEM_TFD_OFFSET)
#define IWL_SDIO_8000_SF_MEM_TFDI_BASE_ADDR \
	(IWL_SDIO_8000_SF_MEM_BASE_ADDR + IWL_SDIO_SF_MEM_TFDI_OFFSET)
#define IWL_SDIO_8000_SF_MEM_BC_BASE_ADDR \
	(IWL_SDIO_8000_SF_MEM_BASE_ADDR + IWL_SDIO_SF_MEM_BC_OFFSET)
#define IWL_SDIO_8000_SF_MEM_TG_BUF_BASE_ADDR \
	(IWL_SDIO_8000_SF_MEM_BASE_ADDR + IWL_SDIO_SF_MEM_TG_BUF_OFFSET)
#define IWL_SDIO_8000_SF_MEM_ADMA_DSC_MEM_BASE \
	(IWL_SDIO_8000_SF_MEM_BASE_ADDR + IWL_SDIO_SF_MEM_ADMA_DSC_OFFSET)
#define IWL_SDIO_8000_SF_MEM_TB_BASE_ADDR \
	(IWL_SDIO_8000_SF_MEM_BASE_ADDR + IWL_SDIO_SF_MEM_TB_OFFSET)

/* SF adresses for device 7000 */
#define IWL_SDIO_7000_SF_MEM_SIZE	0x20000

#define IWL_SDIO_7000_SF_MEM_BASE_ADDR		0x80000

#define IWL_SDIO_7000_SF_MEM_TFD_BASE_ADDR \
	(IWL_SDIO_7000_SF_MEM_BASE_ADDR + IWL_SDIO_SF_MEM_TFD_OFFSET)
#define IWL_SDIO_7000_SF_MEM_TFDI_BASE_ADDR \
	(IWL_SDIO_7000_SF_MEM_BASE_ADDR + IWL_SDIO_SF_MEM_TFDI_OFFSET)
#define IWL_SDIO_7000_SF_MEM_BC_BASE_ADDR \
	(IWL_SDIO_7000_SF_MEM_BASE_ADDR + IWL_SDIO_SF_MEM_BC_OFFSET)
#define IWL_SDIO_7000_SF_MEM_TG_BUF_BASE_ADDR \
	(IWL_SDIO_7000_SF_MEM_BASE_ADDR + IWL_SDIO_SF_MEM_TG_BUF_OFFSET)
#define IWL_SDIO_7000_SF_MEM_ADMA_DSC_MEM_BASE \
	(IWL_SDIO_7000_SF_MEM_BASE_ADDR + IWL_SDIO_SF_MEM_ADMA_DSC_OFFSET)
#define IWL_SDIO_7000_SF_MEM_TB_BASE_ADDR \
	(IWL_SDIO_7000_SF_MEM_BASE_ADDR + IWL_SDIO_SF_MEM_TB_OFFSET)

#define IWL_SDIO_TFD_POOL_SIZE 128
#define IWL_SDIO_DEFAULT_TB_POOL_SIZE 325

/*
 * SDIO ADMA (AL) registers and config values
 */
#define IWL_SDIO_SF_MEM_ADMA_DSC_LENGTH		256
#define IWL_SDIO_SF_MEM_ADMA_DESC_MEM_PAD_LSB	0x414D4441
#define IWL_SDIO_SF_MEM_ADMA_DESC_MEM_PAD_MSB	0x47554232
#define IWL_SDIO_PADDING_TERMINAL		0xACACACAC
#define IWL_SDIO_CMD_PAD_BYTE			0xAC
#define IWL_SDIO_TRASH_BUF_REG			0x00010400

/* ICCM 0x000000 - 0x0FFFFF   1024KB */
#define IWL_SDIO_ICCM_START_ADDRESS		(0x000000)
/* DCCM 0x800000 - 0x8FFFFF   1024KB */
#define IWL_SDIO_DCCM_START_ADDRESS		(0x800000)
/* PRPH 0xA00000 - 0xAFFFFF   1024KB */
#define IWL_SDIO_PRPH_START_ADDRESS		(0xA00000)
/* Write by TARGET [16:0] In direct auto increment memory read address.
 * The address is in bytes and incremented in 4 bytes (DW).*/
#define IWL_SDIO_HBUS_TARG_MEM_RADD		(0x40C)
/* Write by TARGET [16:0] In direct auto increment memory write address.
 * The address is in bytes and incremented in 4 bytes (DW).*/
#define IWL_SDIO_HBUS_TARG_MEM_WADD		(0x410)
/* Write by TARGET [31:0] Target write data to SRAM. */
#define IWL_SDIO_HBUS_TARG_MEM_WDAT		(0x418)
/* Read by TARGET  [31:0] Target read data to SRAM.*/
#define IWL_SDIO_HBUS_TARG_MEM_RDAT		(0x41C)
/* Write by TARGET [19:0]
 * Target write address for Nevo periphery decoder write operation. */
#define IWL_SDIO_HBUS_TARG_PRPH_WADDR	(0x444)
/* Write by TARGET [19:0]
 * Target write address for Nevo periphery decoder read operation. */
#define IWL_SDIO_HBUS_TARG_PRPH_RADDR		(0x448)
/* Write by TARGET [31:0] Target write data to periphery. */
#define IWL_SDIO_HBUS_TARG_PRPH_WDAT		(0x44C)
/* Read by TARGET  [31:0] Target read data from periphery.*/
#define IWL_SDIO_HBUS_TARG_PRPH_RDAT		(0x450)

/* Target access data address for SDTM */
#define IWL_SDIO_DATA_ADDR			0x0

/*
 * OP Codes used in the commands to the SDTM
 */
enum iwl_sdio_cmd_op_code {
	IWL_SDIO_OP_CODE_READ		= 0x1,
	IWL_SDIO_OP_CODE_WRITE		= 0x2,
	IWL_SDIO_OP_CODE_FW_LOAD	= 0x8,
	IWL_SDIO_OP_CODE_TX_DATA	= 0x8,
	IWL_SDIO_OP_CODE_RX_DATA	= 0x9,
	IWL_SDIO_OP_CODE_MSK		= 0xf,
};

/*
 * SF memory addresses.
 */
struct iwl_sdio_sf_mem_addresses {
	u32 tfd_base_addr;
	u32 tfdi_base_addr;
	u32 bc_base_addr;
	u32 tg_buf_base_addr;
	u32 adma_dsc_mem_base;
	u32 tb_base_addr;
};

/* Marks the end of the transaction in the command to the SDTM */
#define IWL_SDIO_EOT_BIT			BIT(7)
#define IWL_SDIO_CMD_HEADER_SIGNATURE		0x5057
#define IWL_SDIO_OP_CODE_MASK			0xF
#define IWL_SDIO_DMA_DESC_LEN_SHIFT		20
#define IWL_SDIO_MAX_ORDER			(8 * sizeof(u32) - \
						 IWL_SDIO_DMA_DESC_LEN_SHIFT)
#define IWL_SDIO_DMA_DESC_8000_ADDR_SHIFT	32
#define IWL_SDIO_DMA_DESC_8000_LEN_SHIFT	16
#define IWL_SDIO_MAX_ORDER_8000	(8 * sizeof(u32) - \
					 IWL_SDIO_DMA_DESC_8000_LEN_SHIFT)


/*
 * Target Access command structs
 *
 *Command Bits:
 *	Bits 3:0	Op-code
 *	Bits 6:4	Reserved
 *	Bit 7		EOT
 *	Bits 15:8	Sequence number
 *	Bits 31:16	Signature (0x5057)
 */
struct iwl_sdio_cmd_header {
	u8	op_code;
	u8	seq_number;
	__le16	signature;
} __packed;

/*************************
 *	Target Access
 **************************/

/*
 * Enum for SDIO Access width field in the target access command
 */
enum iwl_sdio_ta_width {
	IWL_SDIO_TA_AW_ONE_BYTE		= 0,
	IWL_SDIO_TA_AW_TWO_BYTE		= BIT(24),
	IWL_SDIO_TA_AW_THREE_BYTE	= BIT(25),
	IWL_SDIO_TA_AW_FOUR_BYTE	= BIT(24) | BIT(25),
};

/*
 * This flag defines the three mode of the accesss control that can be
 * done by the SDTM in the tarrrget access command.
*/
enum iwl_sdio_ta_ac_flags {
	IWL_SDIO_TA_AC_DIRECT,
	IWL_SDIO_TA_AC_INDIRECT,
	IWL_SDIO_TA_AC_PRPH,
};

/* Shift in access control field for the data register address */
#define IWL_SDIO_CTRL_DATA_SHIFT		12
#define IWL_SDIO_CTRL_INDIRECT_BIT		BIT(24)
#define IWL_SDIO_CTRL_FIXED_ADDR_BIT		BIT(25)
#define IWL_SDIO_TA_MAX_LENGTH			0xFFF
#define IWL_SDIO_TA_MAX_ADDRESS			0xFFFFFF

/*
 * Target access command to the SDTM.
 *
 * Header: Specified in the cmd header.
 * Address
 *	Bits 23:0	DW aligned address
 *	Bits 25:24
 *		Access width (1, 2, 4 bytes)
 *			00 - one byte, 01 - two bytes,
 *			10 - three bytes,11 - full bytes
 *	Bits 31:26	Reserved
 * Length
 *	Bits 11:0	Length (bytes) (DW aligned).
 *	Bits 31:12	Reserved
 * Access Control
 *	Bits 11:0	ADDR REG address
 *	Bits 23:12	DATA REG address
 *	Bit 24		Access Mode (Direct=0/Indirect=1)
 *	Bit 25		Auto Increment=0 / Fixed Address=1
 *	Bits 31:26	Reserved
 * Buffer
 * Padding
 *	Aligned to transaction size (filled with 0xAC)
 */
struct iwl_sdio_ta_cmd {
	struct iwl_sdio_cmd_header hdr;
	__le32 address;
	__le32 length;
	__le32 access_control;
} __packed;

/* Max payload for a single transaction - dependant in the SDIO BLOCK SIZE */
#define IWL_SDIO_MAX_PAYLOAD_SIZE \
		(IWL_SDIO_BLOCK_SIZE - sizeof(struct iwl_sdio_ta_cmd))

/*
 * This represents a struct used for commands to the SDTM.
 *
 * It contains the header for the SDTM parsing and the payload,
 * which should be up to BLOCK SIZE and padded if the data is
 * not of that size.
*/
struct iwl_sdio_cmd_buffer {
	struct iwl_sdio_ta_cmd ta_cmd;
	u8 payload[IWL_SDIO_MAX_PAYLOAD_SIZE];
} __packed;

/*************************
 *	Data Path
 **************************/

/*
 * RX data command from the AL.
 *
 * Command
 *		Bits 3:0	(Op-code = 0x9)
 *		Bits 6:4	Reserved
 *		Bit 7		EOT
 *		Bits 15:8	Sequence number
 *		Bits 31:16	Signature (0x5057)
 * BUF-DESC	Bits 23:0	Length (Bytes)
 *		Bits 31:24	Reserved
 * Reserved	8 Byte	Reserved
 */
struct iwl_sdio_rx_cmd {
	struct iwl_sdio_cmd_header hdr;
	__le32 length;
	__le64 reserved;
} __packed;

/*
 * This represents a data path rx packet from the SDTM.
 *
 * It contains the header form the SDTM and the payload,
 * The size is set by the configuration in the SDTM (RB size).
 *
*/
struct iwl_sdio_rx_buffer {
	struct iwl_sdio_rx_cmd rx_cmd;
	struct iwl_rx_packet pkt;
} __packed;

#endif /* __iwl_sdio_al_h__ */
