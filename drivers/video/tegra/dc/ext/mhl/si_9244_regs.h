
/***********************************************************************************/
/*  Copyright (c) 2010-2011, Silicon Image, Inc.  All rights reserved.             */
/*  Copyright (C) 2016 XiaoMi, Inc.                                                */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1140 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/
/*
	@file: si_9244_regs.h
 */

///////////////////////////////////////////////////////////////////////////////
//
// CBUS register defintions
//
#define REG_CBUS_INTR_STATUS            0x08
#define BIT_DDC_ABORT                   (BIT2)    /* Responder aborted DDC command at translation layer */
#define BIT_MSC_MSG_RCV                 (BIT3)    /* Responder sent a VS_MSG packet (response data or command.) */
#define BIT_MSC_XFR_DONE                (BIT4)    /* Responder sent ACK packet (not VS_MSG) */
#define BIT_MSC_XFR_ABORT               (BIT5)    /* Command send aborted on TX side */
#define BIT_MSC_ABORT                   (BIT6)    /* Responder aborted MSC command at translation layer */

#define REG_CBUS_INTR_ENABLE            0x09

#define REG_DDC_ABORT_REASON        	0x0C
#define REG_CBUS_BUS_STATUS             0x0A
#define BIT_BUS_CONNECTED                   0x01
#define BIT_LA_VAL_CHG                      0x02

#define REG_PRI_XFR_ABORT_REASON        0x0D

#define REG_CBUS_PRI_FWR_ABORT_REASON   0x0E
#define	CBUSABORT_BIT_REQ_MAXFAIL			(0x01 << 0)
#define	CBUSABORT_BIT_PROTOCOL_ERROR		(0x01 << 1)
#define	CBUSABORT_BIT_REQ_TIMEOUT			(0x01 << 2)
#define	CBUSABORT_BIT_UNDEFINED_OPCODE		(0x01 << 3)
#define	CBUSSTATUS_BIT_CONNECTED			(0x01 << 6)
#define	CBUSABORT_BIT_PEER_ABORTED			(0x01 << 7)

#define REG_CBUS_PRI_START              0x12
#define BIT_TRANSFER_PVT_CMD                0x01
#define BIT_SEND_MSC_MSG                    0x02
#define	MSC_START_BIT_MSC_CMD		        (0x01 << 0)
#define	MSC_START_BIT_VS_CMD		        (0x01 << 1)
#define	MSC_START_BIT_READ_REG		        (0x01 << 2)
#define	MSC_START_BIT_WRITE_REG		        (0x01 << 3)
#define	MSC_START_BIT_WRITE_BURST	        (0x01 << 4)

#define REG_CBUS_PRI_ADDR_CMD           0x13
#define REG_CBUS_PRI_WR_DATA_1ST        0x14
#define REG_CBUS_PRI_WR_DATA_2ND        0x15
#define REG_CBUS_PRI_RD_DATA_1ST        0x16
#define REG_CBUS_PRI_RD_DATA_2ND        0x17


#define REG_CBUS_PRI_VS_CMD             0x18
#define REG_CBUS_PRI_VS_DATA            0x19

#define	REG_MSC_WRITE_BURST_LEN         0x20
#define	MSC_REQUESTOR_DONE_NACK         	(0x01 << 6)

#define	REG_CBUS_MSC_RETRY_INTERVAL			0x1A
#define	REG_CBUS_DDC_FAIL_LIMIT				0x1C
#define	REG_CBUS_MSC_FAIL_LIMIT				0x1D
#define	REG_CBUS_MSC_INT2_STATUS        	0x1E
#define REG_CBUS_MSC_INT2_ENABLE             	0x1F
#define	MSC_INT2_REQ_WRITE_MSC              (0x01 << 0)
#define	MSC_INT2_HEARTBEAT_MAXFAIL          (0x01 << 1)

#define	REG_MSC_WRITE_BURST_LEN         0x20

#define	REG_MSC_HEARTBEAT_CONTROL       0x21
#define	MSC_HEARTBEAT_PERIOD_MASK		    0x0F
#define	MSC_HEARTBEAT_FAIL_LIMIT_MASK	    0x70
#define	MSC_HEARTBEAT_ENABLE			    0x80

#define REG_MSC_TIMEOUT_LIMIT           0x22
#define	MSC_TIMEOUT_LIMIT_MSB_MASK	        (0x0F)
#define	MSC_LEGACY_BIT					    (0x01 << 7)

#define	REG_CBUS_LINK_CONTROL_1				0x30
#define	REG_CBUS_LINK_CONTROL_2				0x31
#define	REG_CBUS_LINK_CONTROL_3				0x32
#define	REG_CBUS_LINK_CONTROL_4				0x33
#define	REG_CBUS_LINK_CONTROL_5				0x34
#define	REG_CBUS_LINK_CONTROL_6				0x35
#define	REG_CBUS_LINK_CONTROL_7				0x36
#define REG_CBUS_LINK_STATUS_1          	0x37
#define REG_CBUS_LINK_STATUS_2          	0x38
#define	REG_CBUS_LINK_CONTROL_8				0x39
#define	REG_CBUS_LINK_CONTROL_9				0x3A
#define	REG_CBUS_LINK_CONTROL_10				0x3B
#define	REG_CBUS_LINK_CONTROL_11				0x3C
#define	REG_CBUS_LINK_CONTROL_12				0x3D


#define REG_CBUS_LINK_CTRL9_0           0x3A
#define REG_CBUS_LINK_CTRL9_1           0xBA

#define	REG_CBUS_DRV_STRENGTH_0				0x40
#define	REG_CBUS_DRV_STRENGTH_1				0x41
#define	REG_CBUS_ACK_CONTROL				0x42
#define	REG_CBUS_CAL_CONTROL				0x43

#define REG_CBUS_SCRATCHPAD_0           0xC0
#define REG_CBUS_DEVICE_CAP_0           0x80
#define REG_CBUS_DEVICE_CAP_1           0x81
#define REG_CBUS_DEVICE_CAP_2           0x82
#define REG_CBUS_DEVICE_CAP_3           0x83
#define REG_CBUS_DEVICE_CAP_4           0x84
#define REG_CBUS_DEVICE_CAP_5           0x85
#define REG_CBUS_DEVICE_CAP_6           0x86
#define REG_CBUS_DEVICE_CAP_7           0x87
#define REG_CBUS_DEVICE_CAP_8           0x88
#define REG_CBUS_DEVICE_CAP_9           0x89
#define REG_CBUS_DEVICE_CAP_A           0x8A
#define REG_CBUS_DEVICE_CAP_B           0x8B
#define REG_CBUS_DEVICE_CAP_C           0x8C
#define REG_CBUS_DEVICE_CAP_D           0x8D
#define REG_CBUS_DEVICE_CAP_E           0x8E
#define REG_CBUS_DEVICE_CAP_F           0x8F
#define REG_CBUS_SET_INT_0				0xA0
#define REG_CBUS_SET_INT_1				0xA1
#define REG_CBUS_SET_INT_2				0xA2
#define REG_CBUS_SET_INT_3				0xA3
#define REG_CBUS_WRITE_STAT_0        	0xB0
#define REG_CBUS_WRITE_STAT_1        	0xB1
#define REG_CBUS_WRITE_STAT_2        	0xB2
#define REG_CBUS_WRITE_STAT_3        	0xB3

