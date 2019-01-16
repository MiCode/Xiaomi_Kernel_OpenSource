#define SII_HAL_LINUX_INIT_C
#include <linux/i2c.h>
#include "sii_hal.h"
#include "sii_hal_priv.h"
bool gHalInitedFlag = false;
DEFINE_SEMAPHORE(gIsrLock);
mhlDeviceContext_t gMhlDevice;
halReturn_t HalInitCheck(void)
{
	if (!(gHalInitedFlag)) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Error: Hal layer not currently initialize!\n");
		return HAL_RET_NOT_INITIALIZED;
	}
	return HAL_RET_SUCCESS;
}

halReturn_t HalInit(void)
{
	/* halReturn_t   status; */
	if (gHalInitedFlag) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Error: Hal layer already inited!\n");
		return HAL_RET_ALREADY_INITIALIZED;
	}
	gMhlDevice.driver.driver.name = NULL;
	gMhlDevice.driver.id_table = NULL;
	gMhlDevice.driver.probe = NULL;
	gMhlDevice.driver.remove = NULL;
	gMhlDevice.pI2cClient = NULL;
	gMhlDevice.irqHandler = NULL;
#ifdef RGB_BOARD
	gMhlDevice.ExtDeviceirqHandler = NULL;
#endif
	/*
	   status = HalGpioInit();
	   if(status != HAL_RET_SUCCESS)
	   {
	   return status;
	   }
	 */
	gHalInitedFlag = true;
	return HAL_RET_SUCCESS;
}

halReturn_t HalTerm(void)
{
	halReturn_t retStatus;
	retStatus = HalInitCheck();
	if (retStatus != HAL_RET_SUCCESS) {
		return retStatus;
	}
	/* HalGpioTerm(); */
	gHalInitedFlag = false;
	return retStatus;
}

halReturn_t HalAcquireIsrLock(void)
{
	halReturn_t retStatus;
	int status;
	retStatus = HalInitCheck();
	if (retStatus != HAL_RET_SUCCESS) {
		return retStatus;
	}
	status = down_interruptible(&gIsrLock);
	if (status != 0) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "HalAcquireIsrLock failed to acquire lock\n");
		return HAL_RET_FAILURE;
	}
	return HAL_RET_SUCCESS;
}

halReturn_t HalReleaseIsrLock(void)
{
	halReturn_t retStatus;
	retStatus = HalInitCheck();
	if (retStatus != HAL_RET_SUCCESS) {
		return retStatus;
	}
	up(&gIsrLock);
	return retStatus;
}
