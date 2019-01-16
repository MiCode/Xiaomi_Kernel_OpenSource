#include "si_common.h"
#include "si_cra.h"
#include "si_cra_internal.h"
#include "si_cra_cfg.h"
#include "sii_hal.h"
#include "hdmi_cust.h"
#include "si_mhl_tx_api.h"

#if defined(__KERNEL__)
extern uint8_t I2C_ReadByte(uint8_t deviceID, uint8_t offset);
extern void I2C_WriteByte(uint8_t deviceID, uint8_t offset, uint8_t value);
#endif

#if 0 ///SII_I2C_ADDR == (0x76)
static prefuint_t l_pageInstance[SII_CRA_DEVICE_PAGE_COUNT] = { 1 };
#else ///default is 0x72
static prefuint_t l_pageInstance[SII_CRA_DEVICE_PAGE_COUNT] = { 0 };
#endif
extern pageConfig_t g_addrDescriptor[SII_CRA_MAX_DEVICE_INSTANCES][SII_CRA_DEVICE_PAGE_COUNT];
extern SiiReg_t g_siiRegPageBaseReassign[];
extern SiiReg_t g_siiRegPageBaseRegs[SII_CRA_DEVICE_PAGE_COUNT];
CraInstanceData_t craInstance = {
	0,
	0,
	SII_SUCCESS,
	0,
};

#if !defined(__KERNEL__)
static SiiResultCodes_t CraReadBlockI2c(prefuint_t busIndex, uint8_t deviceId, uint8_t regAddr,
					uint8_t *pBuffer, uint16_t count)
{
	SiiResultCodes_t status = SII_ERR_FAIL;
	do {
		if (I2cSendStart(busIndex, deviceId, &regAddr, 1, false) != PLATFORM_SUCCESS) {
			break;
		}
		if (I2cReceiveStart(busIndex, deviceId, pBuffer, count, true) != PLATFORM_SUCCESS) {
			break;
		}
		status = SII_SUCCESS;
	} while (0);
	return (status);
}

static SiiResultCodes_t CraWriteBlockI2c(prefuint_t busIndex, uint8_t deviceId, uint8_t regAddr,
					 const uint8_t *pBuffer, uint16_t count)
{
	SiiResultCodes_t status = SII_ERR_FAIL;
	do {
		if (I2cSendStart(busIndex, deviceId, &regAddr, 1, false) != PLATFORM_SUCCESS) {
			break;
		}
		if (I2cSendContinue(busIndex, pBuffer, count, true) != PLATFORM_SUCCESS) {
			break;
		}
		status = SII_SUCCESS;
	} while (0);
	return (status);
}
#endif
bool_t SiiCraInitialize(void)
{
	prefuint_t i, index;
	craInstance.lastResultCode = RESULT_CRA_SUCCESS;
		
	for (i = 0; i < SII_CRA_DEVICE_PAGE_COUNT; i++) {
#if 1
    if( get_hdmi_i2c_addr()== 0x76)
		l_pageInstance[i] = 1;
	else
		l_pageInstance[i] = 0;
#else
    #if SII_I2C_ADDR == (0x76)
    		l_pageInstance[i] = 1;
    #else
    		l_pageInstance[i] = 0;
    #endif
#endif
	}
	
	i = 0;
	while (g_siiRegPageBaseReassign[i] != 0xFFFF) {
		index = g_siiRegPageBaseReassign[i] >> 8;
		if ((index < SII_CRA_DEVICE_PAGE_COUNT) && (g_siiRegPageBaseRegs[index] != 0xFF)) {
			SiiRegWrite(g_siiRegPageBaseRegs[index],
				    g_siiRegPageBaseReassign[index] & 0x00FF);
		} else {
			craInstance.lastResultCode = SII_ERR_INVALID_PARAMETER;
			break;
		}
		i++;
	}
	return (craInstance.lastResultCode == RESULT_CRA_SUCCESS);
}

SiiResultCodes_t SiiCraGetLastResult(void)
{
	return (craInstance.lastResultCode);
}

#if 0
bool_t SiiRegInstanceSet(SiiReg_t virtualAddress, prefuint_t newInstance)
{
	prefuint_t va = virtualAddress >> 8;
	craInstance.lastResultCode = RESULT_CRA_SUCCESS;
	if ((va < SII_CRA_DEVICE_PAGE_COUNT) && (newInstance < SII_CRA_MAX_DEVICE_INSTANCES)) {
		l_pageInstance[va] = newInstance;
		return (true);
	}
	craInstance.lastResultCode = SII_ERR_INVALID_PARAMETER;
	return (false);
}
#endif

void SiiRegReadBlock(SiiReg_t virtualAddr, uint8_t *pBuffer, uint16_t count)
{
	uint8_t regOffset = (uint8_t) virtualAddr;
	pageConfig_t *pPage;
#if !defined(__KERNEL__)
	SiiResultCodes_t status = SII_ERR_FAIL;
#endif
	virtualAddr >>= 8;
	pPage = &g_addrDescriptor[l_pageInstance[virtualAddr]][virtualAddr];
#if !defined(__KERNEL__)
	switch (pPage->busType) {
	case DEV_I2C_0:
		status =
		    CraReadBlockI2c(DEV_I2C_0, (uint8_t) pPage->address, regOffset, pBuffer, count);
		break;
	case DEV_I2C_OFFSET:
		status =
		    CraReadBlockI2c(DEV_I2C_0, (uint8_t) pPage->address,
				    regOffset + (uint8_t) (pPage->address >> 8), pBuffer, count);
		break;
	default:
		break;
	}
#else
	{
		/*int   i;
		   for (i=0; i<count; i++)
		   {
		   *pBuffer = I2C_ReadByte((uint8_t)pPage->address, (regOffset + i));
		   ++pBuffer;
		   } */
		I2C_ReadBlock((uint8_t) pPage->address, regOffset, pBuffer, count);
	}
#endif
}

uint8_t SiiRegRead(SiiReg_t virtualAddr)
{
	uint8_t value = 0xFF;
	uint8_t regOffset = (uint8_t) virtualAddr;
	pageConfig_t *pPage;
#if !defined(__KERNEL__)
#error
	SiiResultCodes_t status = SII_ERR_FAIL;
#endif
	virtualAddr >>= 8;
	pPage = &g_addrDescriptor[l_pageInstance[virtualAddr]][virtualAddr];
#if !defined(__KERNEL__)
	switch (pPage->busType) {
	case DEV_I2C_0:
		status = CraReadBlockI2c(DEV_I2C_0, (uint8_t) pPage->address, regOffset, &value, 1);
		break;
	case DEV_I2C_OFFSET:
		status =
		    CraReadBlockI2c(DEV_I2C_0, (uint8_t) pPage->address,
				    regOffset + (uint8_t) (pPage->address >> 8), &value, 1);
		break;
	default:
		break;
	}
#else
	value = I2C_ReadByte((uint8_t) pPage->address, regOffset);
	/* printk("value=0x%x\n", value); */
#endif
	return (value);
}

void SiiRegWriteBlock(SiiReg_t virtualAddr, const uint8_t *pBuffer, uint16_t count)
{
	uint8_t regOffset = (uint8_t) virtualAddr;
	pageConfig_t *pPage;
#if !defined(__KERNEL__)
	SiiResultCodes_t status = SII_ERR_FAIL;
#endif
	virtualAddr >>= 8;
	pPage = &g_addrDescriptor[l_pageInstance[virtualAddr]][virtualAddr];
#if !defined(__KERNEL__)
	switch (pPage->busType) {
	case DEV_I2C_0:
		status =
		    CraWriteBlockI2c(DEV_I2C_0, (uint8_t) pPage->address, regOffset, pBuffer,
				     count);
		break;
	case DEV_I2C_OFFSET:
		status =
		    CraWriteBlockI2c(DEV_I2C_0, (uint8_t) pPage->address,
				     regOffset + (uint8_t) (pPage->address >> 8), pBuffer, count);
		break;
	default:
		break;
	}
#else
	{
		int i;
		for (i = 0; i < count; i++) {
			I2C_WriteByte((uint8_t) pPage->address, (regOffset + i), *pBuffer);
			++pBuffer;
		}
	}
#endif
}

void SiiRegWrite(SiiReg_t virtualAddr, uint8_t value)
{
	uint8_t regOffset = (uint8_t) virtualAddr;
	pageConfig_t *pPage;
#if !defined(__KERNEL__)
	SiiResultCodes_t status = SII_ERR_FAIL;
#endif
	virtualAddr >>= 8;
	pPage = &g_addrDescriptor[l_pageInstance[virtualAddr]][virtualAddr];
#if !defined(__KERNEL__)
	switch (pPage->busType) {
	case DEV_I2C_0:
	case DEV_I2C_1:
	case DEV_I2C_2:
	case DEV_I2C_3:
		status =
		    CraWriteBlockI2c(pPage->busType, (uint8_t) pPage->address, regOffset, &value,
				     1);
		break;
	case DEV_I2C_OFFSET:
	case DEV_I2C_1_OFFSET:
	case DEV_I2C_2_OFFSET:
	case DEV_I2C_3_OFFSET:
		status =
		    CraWriteBlockI2c(pPage->busType - DEV_I2C_OFFSET, (uint8_t) pPage->address,
				     regOffset + (uint8_t) (pPage->address >> 8), &value, 1);
		break;
	default:
		break;
	}
#else
	I2C_WriteByte((uint8_t) pPage->address, regOffset, value);
#endif
}

void SiiRegModify(SiiReg_t virtualAddr, uint8_t mask, uint8_t value)
{
	uint8_t aByte;
	aByte = SiiRegRead(virtualAddr);
	aByte &= (~mask);
	aByte |= (mask & value);
	SiiRegWrite(virtualAddr, aByte);
}

void SiiRegBitsSet(SiiReg_t virtualAddr, uint8_t bitMask, bool_t setBits)
{
	uint8_t aByte;
	aByte = SiiRegRead(virtualAddr);
	aByte = (setBits) ? (aByte | bitMask) : (aByte & ~bitMask);
	SiiRegWrite(virtualAddr, aByte);
}

void SiiRegBitsSetNew(SiiReg_t virtualAddr, uint8_t bitMask, bool_t setBits)
{
	uint8_t newByte, oldByte;
	oldByte = SiiRegRead(virtualAddr);
	newByte = (setBits) ? (oldByte | bitMask) : (oldByte & ~bitMask);
	if (oldByte != newByte) {
		SiiRegWrite(virtualAddr, newByte);
	}
}

void SiiRegEdidReadBlock(SiiReg_t segmentAddr, SiiReg_t virtualAddr, uint8_t *pBuffer,
			 uint16_t count)
{
#if 0
	uint8_t regOffset = (uint8_t) virtualAddr;
	pageConfig_t *pPage;
#if !defined(__KERNEL__)
	SiiResultCodes_t status = SII_ERR_FAIL;
	if ((segmentAddr & 0xFF) != 0) {
		regOffset = (uint8_t) segmentAddr;
		segmentAddr >>= 8;
		pPage = &g_addrDescriptor[l_pageInstance[segmentAddr]][segmentAddr];
		I2cSendStart(pPage->busType, pPage->address, &regOffset, 1, false);
	}
#endif
	regOffset = (uint8_t) virtualAddr;
	virtualAddr >>= 8;
	pPage = &g_addrDescriptor[l_pageInstance[virtualAddr]][virtualAddr];
#if !defined(__KERNEL__)
	status = CraReadBlockI2c(pPage->busType, pPage->address, regOffset, pBuffer, count);
#else
	{
		int i;
		for (i = 0; i < count; i++) {
			*pBuffer =
			    I2C_ReadByte((uint8_t) pPage->address, (uint8_t) (regOffset + i));
			++pBuffer;
		}
	}
#endif
#endif

	uint8_t Seg_regOffset = 0x00;
	uint8_t regOffset = (uint8_t) virtualAddr;
	uint8_t return_value1 = 0, return_value2 = 0;

	if ((segmentAddr & 0xFF) != 0) {
		Seg_regOffset = (uint8_t) segmentAddr;
	}
	regOffset = (uint8_t) virtualAddr;

	TX_DEBUG_PRINT(("Seg_regOffset=0x%x,regOffset=0x%x,count=%d\n", Seg_regOffset, regOffset,
			count));
	if (Seg_regOffset == 0)
		return_value1 = I2C_ReadBlock(0xA0, regOffset, pBuffer, count);
	else
		return_value2 =
		    I2C_ReadSegmentBlockEDID(0xA0, Seg_regOffset, regOffset, pBuffer, count);
	if (return_value1 > 0)
		TX_DEBUG_PRINT(("IIC_SCL_TIMEOUT,return_value1\n"));
	if (return_value2 > 0)
		TX_DEBUG_PRINT(("IIC_SCL_TIMEOUT,return_value2\n"));

}
