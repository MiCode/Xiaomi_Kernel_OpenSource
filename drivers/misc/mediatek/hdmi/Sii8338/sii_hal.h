#if !defined(SII_HAL_H)
#define SII_HAL_H
#include <linux/kernel.h>
#include "defs.h"
#include "osal/include/osal.h"
#include "si_osdebug.h"
///#include "hdmitx_i2c.h"


#ifdef __cplusplus
extern "C" {
#endif
#ifndef	FALSE
#define	FALSE	false
#endif
#ifndef	TRUE
#define	TRUE	true
#endif
#ifndef	BIT_0
#define	BIT_0	(1 << 0)
#define	BIT_1	(1 << 1)
#define	BIT_2	(1 << 2)
#define	BIT_3	(1 << 3)
#define	BIT_4	(1 << 4)
#define	BIT_5	(1 << 5)
#define	BIT_6	(1 << 6)
#define	BIT_7	(1 << 7)
#endif
#ifndef	BIT0
#define	BIT0	BIT_0
#define	BIT1	BIT_1
#define	BIT2	BIT_2
#define	BIT3	BIT_3
#define	BIT4	BIT_4
#define	BIT5	BIT_5
#define	BIT6	BIT_6
#define	BIT7	BIT_7
#endif
	typedef void (*fwIrqHandler_t) (void);
	typedef uint8_t(*fnCheckDevice) (uint8_t dev);
#if defined(ROM)
#undef	ROM
#endif
#define	ROM
	typedef void (*timerCallbackHandler_t) (void *);
	typedef enum {
		HAL_RET_SUCCESS,
		HAL_RET_FAILURE,
		HAL_RET_PARAMETER_ERROR,
		HAL_RET_NO_DEVICE,
		HAL_RET_DEVICE_NOT_OPEN,
		HAL_RET_NOT_INITIALIZED,
		HAL_RET_OUT_OF_RESOURCES,
		HAL_RET_TIMEOUT,
		HAL_RET_ALREADY_INITIALIZED
	} halReturn_t;
#define ELAPSED_TIMER               0xFF
#define ELAPSED_TIMER1              0xFE
	typedef enum TimerId {
		TIMER_0 = 0,
		TIMER_POLLING,
		TIMER_2,
		TIMER_3,
		TIMER_4,
		TIMER_5,
		TIMER_COUNT
	} timerId_t;
#define settingMode3X				0
#define settingMode1X				1
#define settingMode9290				0
#define settingMode938x				1
#define GPIO_PIN_SW5_P4_ENABLED		0
#define GPIO_PIN_SW5_P4_DISABLED	1
	typedef enum {
		GPIO_INT = 0x00,
		GPIO_RST,
		GPIO_M2U_VBUS_CTRL,
		GPIO_V_INT,
		GPIO_REQ_IN,
		GPIO_GNT,
		GPIO_MHL_USB,
		GPIO_SRC_VBUS_ON,
		GPIO_SINK_VBUS_ON,
		GPIO_VBUS_EN,
		GPIO_INVALID = 0xFF
	} GpioIndex_t;
	halReturn_t HalInit(void);
	halReturn_t HalTerm(void);
	halReturn_t HalOpenI2cDevice(char const *DeviceName, char const *DriverName);
	halReturn_t HalCloseI2cDevice(void);
	halReturn_t HalSmbusReadByteData(uint8_t command, uint8_t *pRetByteRead);
	halReturn_t HalSmbusWriteByteData(uint8_t command, uint8_t writeByte);
	halReturn_t HalSmbusReadWordData(uint8_t command, uint16_t *pRetWordRead);
	halReturn_t HalSmbusWriteWordData(uint8_t command, uint16_t wordData);
	halReturn_t HalSmbusReadBlock(uint8_t command, uint8_t *buffer, uint8_t *bufferLen);
	halReturn_t HalSmbusWriteBlock(uint8_t command, uint8_t const *blockData, uint8_t length);
	halReturn_t HalI2cMasterWrite(uint8_t i2cAddr, uint8_t length, uint8_t *buffer);
	halReturn_t HalI2cMasterRead(uint8_t i2cAddr, uint8_t length, uint8_t *buffer);
	uint8_t I2C_ReadByte(uint8_t deviceID, uint8_t offset);
	void I2C_WriteByte(uint8_t deviceID, uint8_t offset, uint8_t value);
	uint8_t I2C_ReadBlock(uint8_t deviceID, uint8_t offset, uint8_t *buf, uint8_t len);
	uint8_t I2C_ReadSegmentBlockEDID(uint8_t SlaveAddr, uint8_t Segment, uint8_t Offset,
					 uint8_t *Buffer, uint8_t Length);
	halReturn_t HalInstallIrqHandler(fwIrqHandler_t irqHandler);
	halReturn_t HalInstallSilMonRequestIrqHandler(void);
	halReturn_t HalInstallSilExtDeviceIrqHandler(fwIrqHandler_t irqHandler);
	void HalEnableIrq(uint8_t bEnable);
	halReturn_t HalRemoveIrqHandler(void);
	halReturn_t HalRemoveSilMonRequestIrqHandler(void);
	halReturn_t HalRemoveSilExtDeviceIrqHandler(void);
	halReturn_t HalInstallCheckDeviceCB(fnCheckDevice fn);
	void HalTimerSet(uint8_t index, uint16_t m_sec);
	void HalTimerWait(uint16_t m_sec);
	uint8_t HalTimerExpired(uint8_t timerIndex);
	uint16_t HalTimerElapsed(uint8_t elapsedTimerIndex);
	halReturn_t HalGpioSetPin(GpioIndex_t gpio, int value);
	halReturn_t HalGpioGetPin(GpioIndex_t gpio, int *value);
	halReturn_t HalGetGpioIrqNumber(GpioIndex_t gpio, unsigned int *irqNumber);
	halReturn_t HalEnableI2C(int bEnable);
	halReturn_t HalAcquireIsrLock(void);
	halReturn_t HalReleaseIsrLock(void);
#ifdef __cplusplus
}
#endif
#endif
