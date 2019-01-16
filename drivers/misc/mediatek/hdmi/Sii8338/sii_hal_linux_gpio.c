#define SII_HAL_LINUX_GPIO_C
#include "sii_hal.h"
#include "sii_hal_priv.h"
#include <linux/ioport.h>
#include <asm/io.h>
unsigned char pinDbgMsgs = 0;
unsigned char pinOverrideTiming = 0;
unsigned char pinDbgSw3 = 0;
unsigned char pinDbgSw4 = 0;
unsigned char pinDbgSw5 = 0;
unsigned char pinDbgSw6 = 0;
unsigned char pinSw = 0;
unsigned char pinPwSw1aEn = 0;
unsigned char pinM2uVbusCtrlM = 0;
unsigned char pinMhlUsb = 0;
typedef enum {
	DIRECTION_IN = 0,
	DIRECTION_OUT,
} GPIODirection_t;
typedef struct tagGPIOInfo {
	GpioIndex_t index;
	int gpio_number;
	char gpio_descripion[40];
	GPIODirection_t gpio_direction;
	int init_value;
} GPIOInfo_t;
#define GPIO_ITEM(a, b, c, d) \
{.index = (a),\
 .gpio_number = (b), \
 .gpio_descripion = (#a), \
 .gpio_direction = (c), \
 .init_value = (d),  }
static GPIOInfo_t GPIO_List[] = {
	GPIO_ITEM(GPIO_INT, 137, DIRECTION_IN, 0),
	GPIO_ITEM(GPIO_RST, 138, DIRECTION_OUT, 1),
	/*
	   GPIO_ITEM(GPIO_M2U_VBUS_CTRL,   139,DIRECTION_OUT,1),
	   GPIO_ITEM(GPIO_V_INT,           132,DIRECTION_IN,0),
	   GPIO_ITEM(GPIO_REQ_IN,          136,DIRECTION_IN,0),
	   GPIO_ITEM(GPIO_GNT,             135,DIRECTION_OUT,1),
	   GPIO_ITEM(GPIO_MHL_USB,         131,DIRECTION_OUT,1),
	   GPIO_ITEM(GPIO_SRC_VBUS_ON,     133,DIRECTION_OUT,0),
	   GPIO_ITEM(GPIO_SINK_VBUS_ON,    134,DIRECTION_OUT,0),
	   GPIO_ITEM(GPIO_VBUS_EN,         130,DIRECTION_IN,0),
	 */
};

static GPIOInfo_t *GetGPIOInfo(int gpio)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(GPIO_List); i++) {
		if (gpio == GPIO_List[i].index) {
			return &GPIO_List[i];
		}
	}
	return NULL;
}

#define IEN     (1 << 8)
#define IDIS    (0 << 8)
#define PTU     (1 << 4)
#define PTD     (0 << 4)
#define EN      (1 << 3)
#define DIS     (0 << 3)
#define M0      0
#define M1      1
#define M2      2
#define M3      3
#define M4      4
#define M5      5
#define M6      6
#define M7      7
#define IO_PHY_ADDRESS  0x48000000
#define PAD_CONF_OFFSET  0x2030
static void HalSetPinMux(void)
{
#define PADCONF_SDMMC2_CLK_OFFSET			0x128
	int i;
	unsigned short x, old;
	void *base = NULL;
	base = ioremap(IO_PHY_ADDRESS, 0x10000);
	if (base == NULL) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "IO Mapping failed\n");
		return;
	} else {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "iobase = 0x%x\n", base);
	}
	for (i = 0; i < 10; i++) {
		old = ioread16(base + PAD_CONF_OFFSET + PADCONF_SDMMC2_CLK_OFFSET + i * 2);
		switch (i) {
		case 7:
		case 0:
		case 6:
		case 2:
			x = (IEN | EN | PTU | M4);
			break;
		default:
			x = (M4 | IDIS);
			break;
		}
		iowrite16(x, base + PAD_CONF_OFFSET + PADCONF_SDMMC2_CLK_OFFSET + i * 2);
	}
	iounmap(base);
}

halReturn_t HalEnableI2C(int bEnable)
{
#define PADCONF_I2C2_SCL_OFFSET			0x18e
#define PADCONF_I2C2_SDA_OFFSET			0x190
	unsigned short x, old;
	void *base = NULL;
	base = ioremap(IO_PHY_ADDRESS, 0x10000);
	if (base == NULL) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "IO Mapping failed\n");
		return HAL_RET_FAILURE;
	}
	old = ioread16(base + PAD_CONF_OFFSET + PADCONF_I2C2_SCL_OFFSET);
	if (bEnable) {
		x = (IEN | PTU | EN | M0);
	} else {
		x = (EN | PTD | M4 | IDIS);
	}
	iowrite16(x, base + PAD_CONF_OFFSET + PADCONF_I2C2_SCL_OFFSET);
	old = ioread16(base + PAD_CONF_OFFSET + PADCONF_I2C2_SDA_OFFSET);
	if (bEnable) {
		x = (IEN | M0);
	} else {
		x = (EN | PTD | M4 | IDIS);
	}
	iowrite16(x, base + PAD_CONF_OFFSET + PADCONF_I2C2_SDA_OFFSET);
	iounmap(base);
	return HAL_RET_SUCCESS;
}

halReturn_t HalGpioInit(void)
{
	int status;
	int i, j;
	HalSetPinMux();
	for (i = 0; i < ARRAY_SIZE(GPIO_List); i++) {
		status = gpio_request(GPIO_List[i].gpio_number, GPIO_List[i].gpio_descripion);
		if (status < 0 && status != -EBUSY) {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"HalInit gpio_request for GPIO %d (H/W Reset) failed, status: %d\n",
					GPIO_List[i].gpio_number, status);
			for (j = 0; j < i; j++) {
				gpio_free(GPIO_List[j].gpio_number);
			}
			return HAL_RET_FAILURE;
		}
		if (GPIO_List[i].gpio_direction == DIRECTION_OUT) {
			status =
			    gpio_direction_output(GPIO_List[i].gpio_number,
						  GPIO_List[i].init_value);
		} else {
			status = gpio_direction_input(GPIO_List[i].gpio_number);
		}
		if (status < 0) {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"HalInit gpio_direction_output for GPIO %d (H/W Reset) failed, status: %d\n",
					GPIO_List[i].gpio_number, status);
			for (j = 0; j <= i; j++) {
				gpio_free(GPIO_List[j].gpio_number);
			}
			return HAL_RET_FAILURE;
		}
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "initialize %s successfully\n",
				GPIO_List[i].gpio_descripion);
	}
	return HAL_RET_SUCCESS;
}

halReturn_t HalGpioTerm(void)
{
	halReturn_t halRet;
	int index;
	halRet = HalInitCheck();
	if (halRet != HAL_RET_SUCCESS) {
		return halRet;
	}
	for (index = 0; index < ARRAY_SIZE(GPIO_List); index++) {
		gpio_free(GPIO_List[index].gpio_number);
	}
	return HAL_RET_SUCCESS;
}

halReturn_t HalGpioSetPin(GpioIndex_t gpio, int value)
{
	halReturn_t halRet;
	GPIOInfo_t *pGpioInfo;
	halRet = HalInitCheck();
	if (halRet != HAL_RET_SUCCESS) {
		return halRet;
	}
	pGpioInfo = GetGPIOInfo(gpio);
	if (!pGpioInfo) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "%d is NOT right gpio_index!\n", (int)gpio);
		return HAL_RET_FAILURE;
	}
	if (pGpioInfo->gpio_direction != DIRECTION_OUT) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "gpio(%d) is NOT ouput gpio!\n",
				pGpioInfo->gpio_number);
		return HAL_RET_FAILURE;
	}
	gpio_set_value(pGpioInfo->gpio_number, value ? 1 : 0);
	if (gpio != GPIO_GNT) {
		if (value) {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, ">> %s to HIGH <<\n",
					pGpioInfo->gpio_descripion);
		} else {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, ">> %s to LOW <<\n",
					pGpioInfo->gpio_descripion);
		}
	}
	return HAL_RET_SUCCESS;
}

halReturn_t HalGpioGetPin(GpioIndex_t gpio, int *value)
{
	halReturn_t halRet;
	GPIOInfo_t *pGpioInfo;
	halRet = HalInitCheck();
	if (halRet != HAL_RET_SUCCESS) {
		return halRet;
	}
	pGpioInfo = GetGPIOInfo(gpio);
	if (!pGpioInfo) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "%d is NOT right gpio_index!\n", (int)gpio);
		return HAL_RET_FAILURE;
	}
	if (pGpioInfo->gpio_direction != DIRECTION_IN) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "gpio(%d) is NOT input gpio!\n",
				pGpioInfo->gpio_number);
		return HAL_RET_FAILURE;
	}
	*value = gpio_get_value(pGpioInfo->gpio_number);
	return HAL_RET_SUCCESS;
}

halReturn_t HalGetGpioIrqNumber(GpioIndex_t gpio, unsigned int *irqNumber)
{
	halReturn_t halRet;
	GPIOInfo_t *pGpioInfo;
	halRet = HalInitCheck();
	if (halRet != HAL_RET_SUCCESS) {
		return halRet;
	}
	pGpioInfo = GetGPIOInfo(gpio);
	if (!pGpioInfo) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "%d is NOT right gpio_index!\n", (int)gpio);
		return HAL_RET_FAILURE;
	}
	*irqNumber = gpio_to_irq(pGpioInfo->gpio_number);
	if (*irqNumber >= 0) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "%s(%d)-->IRQ(%d)\n",
				pGpioInfo->gpio_descripion, pGpioInfo->gpio_number, *irqNumber);
		return HAL_RET_SUCCESS;
	}
	return HAL_RET_FAILURE;
}
