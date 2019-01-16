#if !defined(MHL_DRIVER_IOCTL_H)
#define MHL_DRIVER_IOCTL_H
#include <linux/ioctl.h>
#ifdef __cplusplus
extern "C" {
#endif
	typedef struct tagRegister {
		uint8_t dev_address;
		uint8_t subaddr;
		uint8_t value;
		uint8_t mask;
	} Register_t;
	typedef enum tagUserControlID {
		USER_TRIGGER_EXT_INT = 0x00,
		USER_ON_OFF_MHL_INT,
		USER_RESET_MHL_CHIP,
		USER_READ_SINK_EDID,
		USER_TRIGGER_MHL_INT,
		USER_GPIO_SET,
		USER_GPIO_GET,
	} UserControlID_e;
	typedef struct tagGpioCtrl {
		unsigned char GpioIndex;
		unsigned int Value;
	} GpioCtrl_t;
	typedef struct tagUserControl {
		UserControlID_e ControlID;
		union {
			unsigned char uSubCommand;
			unsigned int iSubCommand;
			unsigned char EDID[256];
			GpioCtrl_t GpioCtrl;
		} SubCommand;
	} UserControl_t;
#define IOC_SII_MHL_TYPE ('S')
#define SII_IOCTRL_REGISTER_READ \
    _IOW(IOC_SII_MHL_TYPE, 0x05, Register_t *)
#define SII_IOCTRL_REGISTER_WRITE \
    _IOW(IOC_SII_MHL_TYPE, 0x06, Register_t *)
#define SII_IOCTRL_USER \
    _IOW(IOC_SII_MHL_TYPE, 0x07, uint8_t)
#ifdef __cplusplus
}
#endif
#endif
