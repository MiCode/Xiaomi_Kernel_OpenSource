/***********************************************************************************/
/*  Copyright (c) 2002-2009, 2011 Silicon Image, Inc.  All rights reserved.        */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1060 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/
#include <linux/types.h>
#include "sii_9244_api.h"


uint8_t     I2C_ReadByte(uint8_t deviceID, uint8_t offset);
void        I2C_WriteByte(uint8_t deviceID, uint8_t offset, uint8_t value);

uint8_t     ReadBytePage0 (uint8_t Offset);
void        WriteBytePage0 (uint8_t Offset, uint8_t Data);
void        ReadModifyWritePage0 (uint8_t Offset, uint8_t Mask, uint8_t Data);

uint8_t     ReadByteCBUS (uint8_t Offset);
void        WriteByteCBUS (uint8_t Offset, uint8_t Data);
void        ReadModifyWriteCBUS(uint8_t Offset, uint8_t Mask, uint8_t Value);

#define CI2CA LOW


#if (CI2CA == LOW)
#define	PAGE_0_0X72			0x72
#define	PAGE_1_0X7A			0x7A
#define	PAGE_2_0X92			0x92
#define	PAGE_CBUS_0XC8		0xC8
#else
#define	PAGE_0_0X72			0x76
#define	PAGE_1_0X7A			0x7E
#define	PAGE_2_0X92			0x96
#define	PAGE_CBUS_0XC8		0xCC
#endif


