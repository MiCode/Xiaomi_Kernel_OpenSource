#if !defined(SII_HAL_PRIV_H)
#define SII_HAL_PRIV_H
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifdef MAKE_8338_DRIVER
#define BASE_I2C_ADDR   0x72
#endif
	typedef struct {
		struct i2c_driver driver;
		struct i2c_client *pI2cClient;
		fwIrqHandler_t irqHandler;
		unsigned int SilMonRequestIRQ;
		spinlock_t SilMonRequestIRQ_Lock;
		unsigned int SilMonControlReleased;
		fnCheckDevice CheckDevice;
#ifdef RGB_BOARD
		unsigned int SilExtDeviceIRQ;
		fwIrqHandler_t ExtDeviceirqHandler;
#endif
	} mhlDeviceContext_t, *pMhlDeviceContext;
	extern bool gHalInitedFlag;
	extern struct i2c_device_id gMhlI2cIdTable[2];
	extern mhlDeviceContext_t gMhlDevice;
	halReturn_t HalInitCheck(void);
	halReturn_t I2cAccessCheck(void);
	halReturn_t HalGpioInit(void);
	halReturn_t HalGpioTerm(void);
#ifdef __cplusplus
}
#endif
#endif
