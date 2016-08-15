/***********************************************************************************/
/*  Copyright (c) 2002-2009, Silicon Image, Inc.  All rights reserved.             */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1060 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/

//#include "si_c99support.h"
//#include "si_memsegsupport.h"
//#include "si_bitdefs.h"
#include "si_mhl_defs.h"
//#include "defs.h"
#include "sii_reg_access.h"

//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadBytePage0 ()
//
// PURPOSE		:	Read the value from a Page0 register.
//
// INPUT PARAMS	:	Offset	-	the offset of the Page0 register to be read.
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	The value read from the Page0 register.
//
//////////////////////////////////////////////////////////////////////////////

uint8_t ReadBytePage0 (uint8_t Offset)
{
	return I2C_ReadByte(PAGE_0_0X72, Offset);
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	WriteBytePage0 ()
//
// PURPOSE		:	Write a value to a Page0 register.
//
// INPUT PARAMS	:	Offset	-	the offset of the Page0 register to be written.
//					Data	-	the value to be written.
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	void
//
//////////////////////////////////////////////////////////////////////////////

void WriteBytePage0 (uint8_t Offset, uint8_t Data)
{
	I2C_WriteByte(PAGE_0_0X72, Offset, Data);
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadModifyWritePage0 ()
//
// PURPOSE		:	Set or clear individual bits in a Page0 register.
//
// INPUT PARAMS	:	Offset	-	the offset of the Page0 register to be modified.
//					Mask	-	"1" for each Page0 register bit that needs to be
//								modified
//					Data	-	The desired value for the register bits in their
//								proper positions
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	void
//
// EXAMPLE		:	If Mask of 0x0C and a
//
//////////////////////////////////////////////////////////////////////////////

void ReadModifyWritePage0(uint8_t Offset, uint8_t Mask, uint8_t Data)
{

	uint8_t Temp;

	Temp = ReadBytePage0(Offset);
	Temp &= ~Mask;
	Temp |= (Data & Mask);
	WriteBytePage0(Offset, Temp);
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadByteCBUS ()
//
// PURPOSE		:	Read the value from a CBUS register.
//
// INPUT PARAMS	:	Offset - the offset of the CBUS register to be read.
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	The value read from the CBUS register.
//
//////////////////////////////////////////////////////////////////////////////

uint8_t ReadByteCBUS (uint8_t Offset)
{
	return I2C_ReadByte(PAGE_CBUS_0XC8, Offset);
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	WriteByteCBUS ()
//
// PURPOSE		:	Write a value to a CBUS register.
//
// INPUT PARAMS	:	Offset	-	the offset of the Page0 register to be written.
//					Data	-	the value to be written.
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	void
//
//////////////////////////////////////////////////////////////////////////////

void WriteByteCBUS(uint8_t Offset, uint8_t Data)
{
	I2C_WriteByte(PAGE_CBUS_0XC8, Offset, Data);
}

//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadModifyWriteCBUS ()
//
// PURPOSE		:	Set or clear individual bits on CBUS page.
//
// INPUT PARAMS	:	Offset	-	the offset of the CBUS register to be modified.
//					Mask	-	"1" for each CBUS register bit that needs to be
//								modified
//					Data	-	The desired value for the register bits in their
//								proper positions
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	void
//
//
//////////////////////////////////////////////////////////////////////////////

void ReadModifyWriteCBUS(uint8_t Offset, uint8_t Mask, uint8_t Value)
{
	uint8_t Temp;

	Temp = ReadByteCBUS(Offset);
	Temp &= ~Mask;
	Temp |= (Value & Mask);
	WriteByteCBUS(Offset, Temp);
}
