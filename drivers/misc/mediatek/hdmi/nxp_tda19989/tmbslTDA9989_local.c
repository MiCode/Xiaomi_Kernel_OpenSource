/**
 * Copyright (C) 2009 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmbslTDA9989_local.c
 *
 * \version       $Revision: 2 $
 *
 *
*/

/*============================================================================*/
/*                   FILE CONFIGURATION                                       */
/*============================================================================*/

/* Defining this symbol on the compiler command line excludes some API checks */
/* #define NO_RETIF_BADPARAM */

/*============================================================================*/
/*                   STANDARD INCLUDE FILES                                   */
/*============================================================================*/


/*============================================================================*/
/*                   PROJECT INCLUDE FILES                                    */
/*============================================================================*/
#include "tmbslHdmiTx_types.h"
#include "tmbslTDA9989_Functions.h"
#include "tmbslTDA9989_local.h"
#ifndef TMFL_TDA19989
#define TMFL_TDA19989
#endif

#ifndef TMFL_NO_RTOS
#define TMFL_NO_RTOS
#endif

#ifndef TMFL_LINUX_OS_KERNEL_DRIVER
#define TMFL_LINUX_OS_KERNEL_DRIVER
#endif


/*============================================================================*/
/*                   MACRO DEFINITIONS                                        */
/*============================================================================*/


/*============================================================================*/
/*                   TYPE DEFINITIONS                                         */
/*============================================================================*/


/*============================================================================*/
/*                   PUBLIC VARIABLE DEFINITIONS                              */
/*============================================================================*/

/** The array of object instances for all concurrently supported HDMI
 *  Transmitter units
 */
#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
#ifdef USE_SHARED_HDMI_INSTANCE
extern tmHdmiTxobject_t gHdmiTxInstance[HDMITX_UNITS_MAX];
#else
#include <linux/kernel.h>
#include <linux/module.h>
tmHdmiTxobject_t gHdmiTxInstance[HDMITX_UNITS_MAX];
EXPORT_SYMBOL(gHdmiTxInstance);
#endif
#else
tmHdmiTxobject_t gHdmiTxInstance[HDMITX_UNITS_MAX];
#endif

/**
 * Lookup table to map register page index to actual page number
 */
UInt8 kPageIndexToPage[E_PAGE_NUM] = {
	0x00,			/* E_PAGE_00 */
	0x01,			/* E_PAGE_01 */
	0x02,			/* E_PAGE_02 */
	0x09,			/* E_PAGE_09 */
	0x10,			/* E_PAGE_10 */
	0x11,			/* E_PAGE_11 */
	0x12,			/* E_PAGE_12 */
	0x13			/* E_PAGE_13 */
};


/*============================================================================*/
/*                   STATIC VARIABLE DECLARATIONS                             */
/*============================================================================*/

/**
 * Lookup table to map an 8-bit mask to a number of left shifts
 * needed to shift a value starting at bit 0 onto the mask.
 * Indexed by mask 0-255. For example, mask 0x00 and 0x01 need
 * no shift, mask 0x02 needs one shift, mask 0x03 needs no shift,
 * mask 0x04 needs 2 shifts, etc.
 * Rows were formatted by "HDMI Driver - Register List.xls" and pasted here
 */
static UInt8 kMaskToShift[256] = {	/* Mask index: */
	/*x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF        */
	0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* 0x */
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* 1x */
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* 2x */
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* 3x */
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* 4x */
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* 5x */
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* 6x */
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* 7x */
	7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* 8x */
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* 9x */
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* Ax */
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* Bx */
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* Cx */
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* Dx */
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,	/* Ex */
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0	/* Fx */
};

/*============================================================================*/
/*                   STATIC FUNCTION DECLARATIONS                             */
/*============================================================================*/

/*============================================================================*/
/*                   PUBLIC FUNCTION DEFINITIONS                              */
/*============================================================================*/

/*============================================================================*/
/* checkUnitSetDis                                                            */
/*============================================================================*/
tmErrorCode_t checkUnitSetDis(tmUnitSelect_t txUnit, tmHdmiTxobject_t **ppDis) {
	/* Return error if unit numbr is out of range */
	RETIF(txUnit < tmUnit0, TMBSL_ERR_HDMI_BAD_UNIT_NUMBER)
	    RETIF(txUnit >= HDMITX_UNITS_MAX, TMBSL_ERR_HDMI_BAD_UNIT_NUMBER)

	    /* Point to unit's Device Instance Structure */
	    * ppDis = &gHdmiTxInstance[txUnit];

	/* Return if this device instance is not initialised */
	if (!(*ppDis)->bInitialized) {
		return TMBSL_ERR_HDMI_NOT_INITIALIZED;
	}

	return TM_OK;
}

#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
EXPORT_SYMBOL(checkUnitSetDis);
#endif

/*============================================================================*/
/* getHwRegisters                                                             */
/*============================================================================*/
tmErrorCode_t
    getHwRegisters(tmHdmiTxobject_t *pDis, UInt16 regShadPageAddr, UInt8 *pData, UInt16 lenData) {
	tmErrorCode_t err;	/* Error code */
	UInt8 regShad;		/* The index to the register's shadow copy */
	UInt8 regPage;		/* The index to the register's page        */
	UInt8 regAddr;		/* The register's address on the page      */
	UInt8 newRegPage;	/* The register's new page number          */
	tmbslHdmiTxSysArgs_t sysArgs;	/* Arguments passed to system function   */

	/* Unpack 1st register's shadow index, page index and address */
	regShad = SPA2SHAD(regShadPageAddr);
	regPage = SPA2PAGE(regShadPageAddr);
	regAddr = SPA2ADDR(regShadPageAddr);
	newRegPage = kPageIndexToPage[regPage];

	/* Check length does not overflow page */
	RETIF_BADPARAM((regAddr + lenData) > E_REG_CURPAGE_ADR_W)

	    /* Check 1st reg does not have a shadow - whole range assumed likewise */
	    RETIF_BADPARAM(regShad != E_SNONE)

	    /* Set page register if required */
	    if (pDis->curRegPage != newRegPage) {
		/* All non-OK results are errors */
		sysArgs.slaveAddr = pDis->uHwAddress;
		sysArgs.firstRegister = E_REG_CURPAGE_ADR_W;
		sysArgs.lenData = 1;
		sysArgs.pData = &newRegPage;
		err = pDis->sysFuncWrite(&sysArgs);
		RETIF(err != TM_OK, TMBSL_ERR_HDMI_I2C_WRITE)
		    pDis->curRegPage = newRegPage;
	}

	/* Get I2C register range - all non-OK results are errors */
	sysArgs.slaveAddr = pDis->uHwAddress;
	sysArgs.firstRegister = regAddr;
	sysArgs.lenData = (UInt8) lenData;
	sysArgs.pData = pData;
	err = pDis->sysFuncRead(&sysArgs);
	return (err == TM_OK) ? TM_OK : TMBSL_ERR_HDMI_I2C_READ;
}

#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
EXPORT_SYMBOL(getHwRegisters);
#endif

/*============================================================================*/
/* getHwRegister                                                              */
/*============================================================================*/
tmErrorCode_t getHwRegister(tmHdmiTxobject_t *pDis, UInt16 regShadPageAddr, UInt8 *pRegValue) {
	tmErrorCode_t err;	/* Error code */
	UInt8 regShad;		/* The index to the register's shadow copy */
	UInt8 regPage;		/* The index to the register's page        */
	UInt8 regAddr;		/* The register's address on the page      */
	UInt8 newRegPage;	/* The register's new page number          */
	tmbslHdmiTxSysArgs_t sysArgs;	/* Arguments passed to system function   */

	/* Unpack register shadow index, page index and address */
	regShad = SPA2SHAD(regShadPageAddr);
	regPage = SPA2PAGE(regShadPageAddr);
	regAddr = SPA2ADDR(regShadPageAddr);
	newRegPage = kPageIndexToPage[regPage];

	/* Set page register if required */
	if (pDis->curRegPage != newRegPage) {
		/* All non-OK results are errors */
		sysArgs.slaveAddr = pDis->uHwAddress;
		sysArgs.firstRegister = E_REG_CURPAGE_ADR_W;
		sysArgs.lenData = 1;
		sysArgs.pData = &newRegPage;
		err = pDis->sysFuncWrite(&sysArgs);
		RETIF(err != TM_OK, TMBSL_ERR_HDMI_I2C_WRITE)
		    pDis->curRegPage = newRegPage;
	}

	if ((regShad != E_SNONE)
	    && (regShadPageAddr != E_REG_P00_INT_FLAGS_0_RW)
	    && (regShadPageAddr != E_REG_P00_INT_FLAGS_1_RW)
	    && (regShadPageAddr != E_REG_P00_INT_FLAGS_2_RW)) {
		/* Get shadow copy - shadowed registers can't be read */
		/* Don't read shadow copy of interrupt status flags! */
		*pRegValue = pDis->shadowReg[regShad];
		return TM_OK;
	} else {
		/* Get I2C register - all non-OK results are errors */
		sysArgs.slaveAddr = pDis->uHwAddress;
		sysArgs.firstRegister = regAddr;
		sysArgs.lenData = 1;
		sysArgs.pData = pRegValue;
		err = pDis->sysFuncRead(&sysArgs);
		return (err == TM_OK) ? TM_OK : TMBSL_ERR_HDMI_I2C_READ;
	}
}

#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
EXPORT_SYMBOL(getHwRegister);
#endif

/*============================================================================*/
/* setHwRegisters                                                             */
/*============================================================================*/
tmErrorCode_t
    setHwRegisters(tmHdmiTxobject_t *pDis, UInt16 regShadPageAddr, UInt8 *pData, UInt16 lenData) {
	tmErrorCode_t err;	/* Error code */
	UInt8 regShad;		/* The index to the register's shadow copy */
	UInt8 regPage;		/* The index to the register's page        */
	UInt8 regAddr;		/* The register's address on the page      */
	UInt8 newRegPage;	/* The register's new page number          */
	tmbslHdmiTxSysArgs_t sysArgs;	/* Arguments passed to system function   */

	/* Unpack 1st register's shadow index, page index and address */
	regShad = SPA2SHAD(regShadPageAddr);
	regPage = SPA2PAGE(regShadPageAddr);
	regAddr = SPA2ADDR(regShadPageAddr);
	newRegPage = kPageIndexToPage[regPage];

	/* Check length does not overflow page */
	RETIF_BADPARAM((regAddr + lenData) > E_REG_CURPAGE_ADR_W)

	    /* Check 1st reg does not have a shadow - whole range assumed likewise */
	    RETIF_BADPARAM(regShad != E_SNONE)

	    /* Set page register if required - whole range is on same page */
	    if (pDis->curRegPage != newRegPage) {
		/* All non-OK results are errors */
		sysArgs.slaveAddr = pDis->uHwAddress;
		sysArgs.firstRegister = E_REG_CURPAGE_ADR_W;
		sysArgs.lenData = 1;
		sysArgs.pData = &newRegPage;
		err = pDis->sysFuncWrite(&sysArgs);
		RETIF(err != TM_OK, TMBSL_ERR_HDMI_I2C_WRITE)
		    pDis->curRegPage = newRegPage;
	}

	/* Write to I2C register range - all non-OK results are errors */
	sysArgs.slaveAddr = pDis->uHwAddress;
	sysArgs.firstRegister = regAddr;
	sysArgs.lenData = (UInt8) lenData;
	sysArgs.pData = pData;
	err = pDis->sysFuncWrite(&sysArgs);
	return (err == TM_OK) ? TM_OK : TMBSL_ERR_HDMI_I2C_WRITE;
}

#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
EXPORT_SYMBOL(setHwRegisters);
#endif


/*============================================================================*/
/* setHwRegisterMsbLsb                                                        */
/*============================================================================*/
tmErrorCode_t setHwRegisterMsbLsb(tmHdmiTxobject_t *pDis, UInt16 regShadPageAddr, UInt16 regWord) {
	tmErrorCode_t err;	/* Error code */
	UInt8 regPage;		/* The index to the register's page        */
	UInt8 regAddr;		/* The register's address on the page      */
	UInt8 newRegPage;	/* The register's new page number          */
	UInt8 msbLsb[2];	/* The bytes from regWord                  */
	tmbslHdmiTxSysArgs_t sysArgs;	/* Arguments passed to system function   */

	/* Unpack register shadow index, page index and address */
	regPage = SPA2PAGE(regShadPageAddr);
	regAddr = SPA2ADDR(regShadPageAddr);
	newRegPage = kPageIndexToPage[regPage];

	/* Unpack regWord bytes, MSB first */
	msbLsb[0] = (UInt8) (regWord >> 8);
	msbLsb[1] = (UInt8) (regWord & 0xFF);

	/* Set page register if required */
	if (pDis->curRegPage != newRegPage) {
		/* All non-OK results are errors */
		sysArgs.slaveAddr = pDis->uHwAddress;
		sysArgs.firstRegister = E_REG_CURPAGE_ADR_W;
		sysArgs.lenData = 1;
		sysArgs.pData = &newRegPage;
		err = pDis->sysFuncWrite(&sysArgs);
		RETIF(err != TM_OK, TMBSL_ERR_HDMI_I2C_WRITE)
		    pDis->curRegPage = newRegPage;
	}

	/* No word registers are shadowed */

	/* Write to I2C - all non-OK results are errors */
	sysArgs.slaveAddr = pDis->uHwAddress;
	sysArgs.firstRegister = regAddr;
	sysArgs.lenData = 2;
	sysArgs.pData = &msbLsb[0];
	err = pDis->sysFuncWrite(&sysArgs);
	return (err == TM_OK) ? TM_OK : TMBSL_ERR_HDMI_I2C_WRITE;
}

#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
EXPORT_SYMBOL(setHwRegisterMsbLsb);
#endif

/*============================================================================*/
/* setHwRegister                                                              */
/*============================================================================*/
tmErrorCode_t setHwRegister(tmHdmiTxobject_t *pDis, UInt16 regShadPageAddr, UInt8 regValue) {
	tmErrorCode_t err;	/* Error code */
	UInt8 regShad;		/* The index to the register's shadow copy */
	UInt8 regPage;		/* The index to the register's page        */
	UInt8 regAddr;		/* The register's address on the page      */
	UInt8 newRegPage;	/* The register's new page number          */
	tmbslHdmiTxSysArgs_t sysArgs;	/* Arguments passed to system function   */

	/* Unpack register shadow index, page index and address */
	regShad = SPA2SHAD(regShadPageAddr);
	regPage = SPA2PAGE(regShadPageAddr);
	regAddr = SPA2ADDR(regShadPageAddr);
	newRegPage = kPageIndexToPage[regPage];

	/* Set page register if required */
	if (pDis->curRegPage != newRegPage) {
		/* All non-OK results are errors */
		sysArgs.slaveAddr = pDis->uHwAddress;
		sysArgs.firstRegister = E_REG_CURPAGE_ADR_W;
		sysArgs.lenData = 1;
		sysArgs.pData = &newRegPage;
		err = pDis->sysFuncWrite(&sysArgs);
		RETIF(err != TM_OK, TMBSL_ERR_HDMI_I2C_WRITE)
		    pDis->curRegPage = newRegPage;
	}

	/* Set shadow copy */
	if (regShad != E_SNONE) {
		pDis->shadowReg[regShad] = regValue;
	}

	/* Write to I2C - all non-OK results are errors */
	sysArgs.slaveAddr = pDis->uHwAddress;
	sysArgs.firstRegister = regAddr;
	sysArgs.lenData = 1;
	sysArgs.pData = &regValue;
	err = pDis->sysFuncWrite(&sysArgs);
	return (err == TM_OK) ? TM_OK : TMBSL_ERR_HDMI_I2C_WRITE;
}

#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
EXPORT_SYMBOL(setHwRegister);
#endif

/*============================================================================*/
/*  setHwRegisterField                                                        */
/*============================================================================*/
tmErrorCode_t
    setHwRegisterField
    (tmHdmiTxobject_t *pDis, UInt16 regShadPageAddr, UInt8 fieldMask, UInt8 fieldValue) {
	tmErrorCode_t err;	/* Error code */
	UInt8 regShad;		/* The index to the register's shadow copy */
	UInt8 regPage;		/* The index to the register's page        */
	UInt8 regAddr;		/* The register's address on the page      */
	UInt8 newRegPage;	/* The register's new page number          */
	UInt8 regValue;		/* The register's current value            */
	tmbslHdmiTxSysArgs_t sysArgs;	/* Arguments passed to system function   */

	/* Unpack register shadow index, page index and address */
	regShad = SPA2SHAD(regShadPageAddr);
	regPage = SPA2PAGE(regShadPageAddr);
	regAddr = SPA2ADDR(regShadPageAddr);
	newRegPage = kPageIndexToPage[regPage];

	/* Set page register if required */
	if (pDis->curRegPage != newRegPage) {
		/* All non-OK results are errors */
		sysArgs.slaveAddr = pDis->uHwAddress;
		sysArgs.firstRegister = E_REG_CURPAGE_ADR_W;
		sysArgs.lenData = 1;
		sysArgs.pData = &newRegPage;
		err = pDis->sysFuncWrite(&sysArgs);
		RETIF(err != TM_OK, TMBSL_ERR_HDMI_I2C_WRITE)
		    pDis->curRegPage = newRegPage;
	}

	if (regShad != E_SNONE) {
		/* Get shadow copy */
		regValue = pDis->shadowReg[regShad];
	} else {
		/* Read I2C register value.
		 * All bitfield registers are either shadowed or can be read.
		 */
		sysArgs.slaveAddr = pDis->uHwAddress;
		sysArgs.firstRegister = regAddr;
		sysArgs.lenData = 1;
		sysArgs.pData = &regValue;
		err = pDis->sysFuncRead(&sysArgs);
		RETIF(err != TM_OK, TMBSL_ERR_HDMI_I2C_READ)
	}

	/* Reset register bits that are set in the mask */
	regValue = regValue & (UInt8) (~fieldMask);

	/* Shift the field value left to align its bits with the mask */
	fieldValue <<= kMaskToShift[fieldMask];

	/* Reset shifted field bits that are not set in the mask */
	fieldValue &= fieldMask;

	/* Set the shifted bitfield */
	regValue |= fieldValue;

	/* Set shadow copy */
	if (regShad != E_SNONE) {
		pDis->shadowReg[regShad] = regValue;
	}

	/* Write to I2C - all non-OK results are errors */
	sysArgs.slaveAddr = pDis->uHwAddress;
	sysArgs.firstRegister = regAddr;
	sysArgs.lenData = 1;
	sysArgs.pData = &regValue;
	err = pDis->sysFuncWrite(&sysArgs);
	return (err == TM_OK) ? TM_OK : TMBSL_ERR_HDMI_I2C_WRITE;
}

#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
EXPORT_SYMBOL(setHwRegisterField);
#endif

/*============================================================================*/
/* getCECHwRegister                                                              */
/*============================================================================*/
tmErrorCode_t getCECHwRegister(tmHdmiTxobject_t *pDis, UInt16 regAddr, UInt8 *pRegValue) {
	tmErrorCode_t err;	/* Error code */
	tmbslHdmiTxSysArgs_t sysArgs;	/* Arguments passed to system function   */
	UInt8 CECAddress;	/* CEC Address deduced from uHwAddress */

	/*Convert HwAddress into CEC Address */
	CECAddress = pDis->uHwAddress;
	CECAddress = CECAddress ^ 0x44;	/*Convert address to obtain  the correspondance in CEC */
	/*CECAddress = ((~CECAddress & 0x44)||(CECAddress & 0xBB)); */
	/*Prepare Write operation */
	sysArgs.slaveAddr = CECAddress;
	sysArgs.firstRegister = (UInt8) regAddr;
	sysArgs.lenData = 1;
	sysArgs.pData = pRegValue;

	/*Read data in Chip */
	err = pDis->sysFuncRead(&sysArgs);	/* Call IC Read function */

	return (err == TM_OK) ? TM_OK : TMBSL_ERR_HDMI_I2C_READ;

}

#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
EXPORT_SYMBOL(getCECHwRegister);
#endif

/*============================================================================*/
/* setCECHwRegister                                                           */
/*============================================================================*/
tmErrorCode_t setCECHwRegister(tmHdmiTxobject_t *pDis, UInt16 regAddr, UInt8 regValue) {
	tmErrorCode_t err;	/* Error code */
	tmbslHdmiTxSysArgs_t sysArgs;	/* Arguments passed to system function   */
	UInt8 CECAddress;	/* CEC Address deduced from uHwAddress */

	/*Convert HwAddress into CEC Address */
	CECAddress = pDis->uHwAddress;
	CECAddress = CECAddress ^ 0x44;	/*Convert address to obtain  the correspondance in CEC */

	/*Prepare Write operation */
	sysArgs.slaveAddr = CECAddress;
	sysArgs.firstRegister = (UInt8) regAddr;
	sysArgs.lenData = 1;
	sysArgs.pData = &regValue;

	/*Write data in Chip */
	err = pDis->sysFuncWrite(&sysArgs);	/* Call IC Write function */

	return (err == TM_OK) ? TM_OK : TMBSL_ERR_HDMI_I2C_WRITE;
}

#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
EXPORT_SYMBOL(setCECHwRegister);
#endif

/*============================================================================*/
/* setHwRegisterFieldTable                                                    */
/*============================================================================*/
tmErrorCode_t setHwRegisterFieldTable(tmHdmiTxobject_t *pDis, const tmHdmiTxRegMaskVal_t *pTable) {
	tmErrorCode_t err;	/* Error code */
	Int i;			/* Table index */

	/* Set register, mask and value from table until terminator is reached */
	for (i = 0; pTable[i].Reg > 0; i++) {
		err = setHwRegisterField(pDis, pTable[i].Reg, pTable[i].Mask, pTable[i].Val);
		RETIF(err != TM_OK, err)
	}
	return TM_OK;
}

#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
EXPORT_SYMBOL(setHwRegisterFieldTable);
#endif


/*============================================================================*/
/* lmemcpy                                                                     */
/*============================================================================*/
tmErrorCode_t lmemcpy(void *pTable1, const void *pTable2, UInt Size) {
	char *ptrSource = (char *)pTable1;
	char *endSource = (char *)pTable1 + Size;
	char *ptrDest = (char *)pTable2;

	RETIF_BADPARAM(pTable1 == Null)
	    RETIF_BADPARAM(pTable2 == Null)

	    while (endSource > ptrSource) {
		*(ptrSource++) = *(ptrDest++);
	}
	return TM_OK;
}

/*============================================================================*/
/* lmemset                                                                     */
/*============================================================================*/
tmErrorCode_t lmemset(void *pTable1, const UInt8 value, UInt Size) {
	char *ptrSource = (char *)pTable1;
	char *endSource = (char *)pTable1 + Size;

	RETIF_BADPARAM(pTable1 == Null)

	    while (endSource > ptrSource) {
		*(ptrSource++) = value;
	}
	return TM_OK;
}

/*============================================================================*/
/*                   STATIC FUNCTION DEFINTIONS                               */
/*============================================================================*/

/*============================================================================*/
/*                     END OF FILE                                            */
/*============================================================================*/
