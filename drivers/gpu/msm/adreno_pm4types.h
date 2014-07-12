/* Copyright (c) 2002,2007-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __ADRENO_PM4TYPES_H
#define __ADRENO_PM4TYPES_H


#define CP_PKT_MASK	0xc0000000

#define CP_TYPE0_PKT	((unsigned int)0 << 30)
#define CP_TYPE1_PKT	((unsigned int)1 << 30)
#define CP_TYPE2_PKT	((unsigned int)2 << 30)
#define CP_TYPE3_PKT	((unsigned int)3 << 30)


/* type3 packets */
/* initialize CP's micro-engine */
#define CP_ME_INIT		0x48

/* skip N 32-bit words to get to the next packet */
#define CP_NOP			0x10

/* indirect buffer dispatch.  same as IB, but init is pipelined */
#define CP_INDIRECT_BUFFER_PFD	0x37

/* wait for the IDLE state of the engine */
#define CP_WAIT_FOR_IDLE	0x26

/* wait until a register or memory location is a specific value */
#define CP_WAIT_REG_MEM	0x3c

/* wait until a register location is equal to a specific value */
#define CP_WAIT_REG_EQ		0x52

/* wait until a register location is >= a specific value */
#define CP_WAT_REG_GTE		0x53

/* wait until a read completes */
#define CP_WAIT_UNTIL_READ	0x5c

/* wait until all base/size writes from an IB_PFD packet have completed */
#define CP_WAIT_IB_PFD_COMPLETE 0x5d

/* register read/modify/write */
#define CP_REG_RMW		0x21

/* Set binning configuration registers */
#define CP_SET_BIN_DATA             0x2f

/* reads register in chip and writes to memory */
#define CP_REG_TO_MEM		0x3e

/* write N 32-bit words to memory */
#define CP_MEM_WRITE		0x3d

/* write CP_PROG_COUNTER value to memory */
#define CP_MEM_WRITE_CNTR	0x4f

/* conditional execution of a sequence of packets */
#define CP_COND_EXEC		0x44

/* conditional write to memory or register */
#define CP_COND_WRITE		0x45

/* generate an event that creates a write to memory when completed */
#define CP_EVENT_WRITE		0x46

/* generate a VS|PS_done event */
#define CP_EVENT_WRITE_SHD	0x58

/* generate a cache flush done event */
#define CP_EVENT_WRITE_CFL	0x59

/* generate a z_pass done event */
#define CP_EVENT_WRITE_ZPD	0x5b


/* initiate fetch of index buffer and draw */
#define CP_DRAW_INDX		0x22

/* draw using supplied indices in packet */
#define CP_DRAW_INDX_2		0x36

/* initiate fetch of index buffer and binIDs and draw */
#define CP_DRAW_INDX_BIN	0x34

/* initiate fetch of bin IDs and draw using supplied indices */
#define CP_DRAW_INDX_2_BIN	0x35

/* New draw packets defined for A4XX */
#define CP_DRAW_INDX_OFFSET	0x38
#define CP_DRAW_INDIRECT	0x28
#define CP_DRAW_INDX_INDIRECT	0x29
#define CP_DRAW_AUTO		0x24

/* begin/end initiator for viz query extent processing */
#define CP_VIZ_QUERY		0x23

/* fetch state sub-blocks and initiate shader code DMAs */
#define CP_SET_STATE		0x25

/* load constant into chip and to memory */
#define CP_SET_CONSTANT	0x2d

/* load sequencer instruction memory (pointer-based) */
#define CP_IM_LOAD		0x27

/* load sequencer instruction memory (code embedded in packet) */
#define CP_IM_LOAD_IMMEDIATE	0x2b

/* load constants from a location in memory */
#define CP_LOAD_CONSTANT_CONTEXT 0x2e

/* (A2x) sets binning configuration registers */
#define CP_SET_BIN_DATA             0x2f

/* selective invalidation of state pointers */
#define CP_INVALIDATE_STATE	0x3b


/* dynamically changes shader instruction memory partition */
#define CP_SET_SHADER_BASES	0x4A

/* sets the 64-bit BIN_MASK register in the PFP */
#define CP_SET_BIN_MASK	0x50

/* sets the 64-bit BIN_SELECT register in the PFP */
#define CP_SET_BIN_SELECT	0x51


/* updates the current context, if needed */
#define CP_CONTEXT_UPDATE	0x5e

/* generate interrupt from the command stream */
#define CP_INTERRUPT		0x40


/* copy sequencer instruction memory to system memory */
#define CP_IM_STORE            0x2c

/* test 2 memory locations to dword values specified */
#define CP_TEST_TWO_MEMS	0x71

/* Write register, ignoring context state for context sensitive registers */
#define CP_REG_WR_NO_CTXT  0x78

/*
 * for A4xx
 * Write to register with address that does not fit into type-0 pkt
 */
#define CP_WIDE_REG_WRITE           0x74


/* PFP waits until the FIFO between the PFP and the ME is empty */
#define CP_WAIT_FOR_ME		0x13

/* Record the real-time when this packet is processed by PFP */
#define CP_RECORD_PFP_TIMESTAMP	0x11

/*
 * for a20x
 * program an offset that will added to the BIN_BASE value of
 * the 3D_DRAW_INDX_BIN packet
 */
#define CP_SET_BIN_BASE_OFFSET     0x4B

/*
 * for a22x
 * sets draw initiator flags register in PFP, gets bitwise-ORed into
 * every draw initiator
 */
#define CP_SET_DRAW_INIT_FLAGS      0x4B

#define CP_SET_PROTECTED_MODE  0x5f /* sets the register protection mode */

#define CP_BOOTSTRAP_UCODE  0x6f /* bootstraps microcode */

/*
 * for a3xx
 */

#define CP_LOAD_STATE 0x30 /* load high level sequencer command */

/* Conditionally load a IB based on a flag */
#define CP_COND_INDIRECT_BUFFER_PFE 0x3A /* prefetch enabled */
#define CP_COND_INDIRECT_BUFFER_PFD 0x32 /* prefetch disabled */

/* Load a buffer with pre-fetch enabled */
#define CP_INDIRECT_BUFFER_PFE 0x3F

#define CP_EXEC_CL 0x31

/* (A4x) save PM4 stream pointers to execute upon a visible draw */
#define CP_SET_DRAW_STATE 0x43

#define CP_LOADSTATE_DSTOFFSET_SHIFT 0x00000000
#define CP_LOADSTATE_STATESRC_SHIFT 0x00000010
#define CP_LOADSTATE_STATEBLOCKID_SHIFT 0x00000013
#define CP_LOADSTATE_NUMOFUNITS_SHIFT 0x00000016
#define CP_LOADSTATE_STATETYPE_SHIFT 0x00000000
#define CP_LOADSTATE_EXTSRCADDR_SHIFT 0x00000002

/* packet header building macros */
#define cp_type0_packet(regindx, cnt) \
	(CP_TYPE0_PKT | (((cnt)-1) << 16) | ((regindx) & 0x7FFF))

#define cp_type0_packet_for_sameregister(regindx, cnt) \
	((CP_TYPE0_PKT | (((cnt)-1) << 16) | ((1 << 15) | \
		((regindx) & 0x7FFF)))

#define cp_type1_packet(reg0, reg1) \
	 (CP_TYPE1_PKT | ((reg1) << 12) | (reg0))

#define cp_type3_packet(opcode, cnt) \
	 (CP_TYPE3_PKT | (((cnt)-1) << 16) | (((opcode) & 0xFF) << 8))

#define cp_predicated_type3_packet(opcode, cnt) \
	 (CP_TYPE3_PKT | (((cnt)-1) << 16) | (((opcode) & 0xFF) << 8) | 0x1)

#define cp_nop_packet(cnt) \
	 (CP_TYPE3_PKT | (((cnt)-1) << 16) | (CP_NOP << 8))

#define pkt_is_type0(pkt) (((pkt) & 0XC0000000) == CP_TYPE0_PKT)

#define type0_pkt_size(pkt) ((((pkt) >> 16) & 0x3FFF) + 1)
#define type0_pkt_offset(pkt) ((pkt) & 0x7FFF)

/*
 * Check both for the type3 opcode and make sure that the reserved bits [1:7]
 * and 15 are 0
 */

#define pkt_is_type3(pkt) \
	((((pkt) & 0xC0000000) == CP_TYPE3_PKT) && \
	 (((pkt) & 0x80FE) == 0))

#define cp_type3_opcode(pkt) (((pkt) >> 8) & 0xFF)
#define type3_pkt_size(pkt) ((((pkt) >> 16) & 0x3FFF) + 1)

/* packet headers */
#define CP_HDR_ME_INIT	cp_type3_packet(CP_ME_INIT, 18)
#define CP_HDR_INDIRECT_BUFFER_PFD cp_type3_packet(CP_INDIRECT_BUFFER_PFD, 2)
#define CP_HDR_INDIRECT_BUFFER_PFE cp_type3_packet(CP_INDIRECT_BUFFER_PFE, 2)

/* dword base address of the GFX decode space */
#define SUBBLOCK_OFFSET(reg) ((unsigned int)((reg) - (0x2000)))

/* gmem command buffer length */
#define CP_REG(reg) ((0x4 << 16) | (SUBBLOCK_OFFSET(reg)))


/* Return 1 if the command is an indirect buffer of any kind */
static inline int adreno_cmd_is_ib(unsigned int cmd)
{
	return (cmd == cp_type3_packet(CP_INDIRECT_BUFFER_PFE, 2) ||
		cmd == cp_type3_packet(CP_INDIRECT_BUFFER_PFD, 2) ||
		cmd == cp_type3_packet(CP_COND_INDIRECT_BUFFER_PFE, 2) ||
		cmd == cp_type3_packet(CP_COND_INDIRECT_BUFFER_PFD, 2));
}

#endif	/* __ADRENO_PM4TYPES_H */
