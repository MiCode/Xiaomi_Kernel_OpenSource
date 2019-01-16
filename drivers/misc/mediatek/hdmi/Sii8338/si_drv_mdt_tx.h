






/*
 SiI8338 Linux Driver

 Copyright (C) 2011 Silicon Image Inc.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation version 2.

 This program is distributed .as is. WITHOUT ANY WARRANTY of any
 kind, whether express or implied; without even the implied warranty
 of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the
 GNU General Public License for more details.
*/
/*
   @file si_drv_mdt_tx.h
 */
/* /#define MDT_SUPPORT 1 */
#define MDT_SUPPORT_DEBUG

#define MHL_INT_ASSERTED_VALUE		0x00

#define OFFSET_SET_INT			0x20
#define OFFSET_SCRATCHPAD		0x40

#define MDT_MIN_PACKET_LENGTH		4
#define MDT_KEYBOARD_PACKET_TAIL_LENGTH	3
#define MDT_KEYBOARD_PACKET_LENGTH	(MDT_MIN_PACKET_LENGTH + MDT_KEYBOARD_PACKET_TAIL_LENGTH)
#define MDT_MAX_PACKET_LENGTH		MDT_KEYBOARD_PACKET_LENGTH


#define MDT_EVENT_HANDLED		1

#define REQ_WRT				(1<<2)
#define GRT_WRT				(1<<3)
#define DSCR_CHG			(1<<1)

enum mdt_state {
	WAIT_FOR_REQ_WRT =
	    0, WAIT_FOR_GRT_WRT_COMPLETE, WAIT_FOR_WRITE_BURST_COMPLETE, WAIT_FOR_REQ_WRT_COMPLETE,
	    WAIT_FOR_GRT_WRT, WAIT_FOR_WRITE_BURST_SENT, IDLE
};


struct msc_request {
	unsigned char offset;
	unsigned char first_data;
};

enum mdt_debug {
	IRQ_HEARTBEAT, IRQ_WAKE, IRQ_RECEIVED, ISR_WRITEBURST_CAUGHT, ISR_WRITEBURST_MISSED,
	    ISR_DEFFER_SCHEDULED, ISR_DEFFER_NO, ISR_DEFFER_BEGIN, ISR_DEFFER_END,
	    I2C_BLOCK_R_UNKNOWN, I2C_BLOCK_W_UNKNOWN, I2C_BLOCK_0x72, I2C_BLOCK_0x7A,
	    I2C_BLOCK_0x92, I2C_BLOCK_0xC8, I2C_BLOCK_0x73, I2C_BLOCK_0x7B, I2C_BLOCK_0x93,
	    I2C_BLOCK_0xC9, I2C_BYTE_0x72, I2C_BYTE_0x7A, I2C_BYTE_0x92, I2C_BYTE_0xC8,
	    I2C_BYTE_0x73, I2C_BYTE_0x7B, I2C_BYTE_0x93, I2C_BYTE_0xC9, ISR_THREADED_BEGIN,
	    ISR_THREADED_END, ISR_MDT_BEGIN, ISR_MDT_END, ISR_FULL_BEGIN, ISR_FULL_END,
	    TOUCHPAD_BEGIN, TOUCHPAD_END, MHL_ESTABLISHED, MSC_READY, MDT_EVENT_PARSED, MDT_UNLOCK,
	    MDT_LOCK, MDT_DISCOVER_REQ
};


#define MSC_PREP_NO_SEND		0
#define MSC_PREP_AND_SEND		1


#ifdef MDT_SUPPORT

unsigned char sii8338_irq_for_mdt(enum mdt_state *mdt_state);

void MHL_log_event(int type, int code, int value);

void mdt_init(void);
void mdt_deregister(void);
#endif
